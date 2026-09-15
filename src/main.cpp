#include <Arduino.h>
#include <cstring>
#include "app_config.h"
#include "display/display_manager.h"
#include "storage/sd_manager.h"
#include "input/touch_manager.h"
#include "system/config_store.h"
#include "system/connection_state.h"
#include "logging/csv_logger.h"
#include "logging/connection_logger.h"
#include "obd/obd_client.h"
#include "obd/obd_credentials.h"
#include "obd/obd_pairing.h"
#include "obd/telemetry.h"
#include "display/cluster_pages.h"
#include "display/touch_handler.h"
#include "display/theme.h"
#include "system/status_led.h"

// Subsystem instances
static DisplayManager displayManager;
static SdManager sdManager;
static TouchManager touchManager(displayManager.getTft(), sdManager);
static ConfigStore configStore(sdManager);
static CsvLogger csvLogger(sdManager);
static ObdClient obdClient;
static ClusterPages clusterPages(displayManager.getTft(), configStore, csvLogger, obdClient);
static ClusterTouchHandler touchHandler(clusterPages, configStore, csvLogger, obdClient, sdManager);
static StatusLed statusLed;

// State variables
static uint32_t lastTouchPollMs = 0;
static bool lastTouchState = false;
static uint32_t lastUiRefreshMs = 0;

static ConnectionState resolveConnectionState(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    ConnectionState transportState = obdClient.getConnectionState();
    if (transportState == ConnectionState::Live && !isFresh(snapshot.rpm, nowMs, config::kTelemetryStaleThresholdMs) &&
        !isFresh(snapshot.speedMph, nowMs, config::kTelemetryStaleThresholdMs)) {
        return ConnectionState::Stale;
    }
    return transportState;
}

void setup() {
    Serial.begin(115200);
    delay(200); // Brief power stabilization
    Serial.println("\n==========================================");
    Serial.println("   CYD 4.0\" ESP32-32E OBD-II Dashboard    ");
    Serial.println("==========================================");

    initializeThemeFonts();

    // 1. Initialize SD Card over VSPI
    sdManager.begin();
    connection_log::beginSession(sdManager); // Start with a fresh connection log each boot

    // 2. Load persisted settings (Config: UI/Gauges/User Vars/Logs pages),
    // falling back to defaults if the SD card or /config.txt is unavailable.
    // Loaded before display/touch init below so the persisted Flip Screen
    // orientation is already known for both the boot logo and touch
    // calibration, rather than being corrected after the fact.
    configStore.begin();
    clusterPages.applyTheme(static_cast<ThemeId>(configStore.settings().themeId));

    // 3. Initialize Display & Backlight, in the persisted orientation
    displayManager.begin(configStore.settings().screenFlipped);

    // Initialize onboard RGB LED (Check Engine / shift-light indicator)
    statusLed.begin();

    // 4. Initialize Touch & Run Calibration if missing from SD (moved ahead
    // of the OBD credentials step below so the Bluetooth pairing screen has
    // working, calibrated touch if it's needed). Runs in the correct
    // orientation, since display rotation is already final.
    touchManager.begin();

    // 5. Load the OBD adapter identity from /obd_config.txt (mac/id/password
    // are all mandatory; there is no fallback). Checked this early - right
    // after display/touch init, before the boot animation/SD logging below -
    // so a doomed boot recovers (or fails fast) before running through
    // several seconds of setup first. A missing/invalid file, or stored
    // credentials that fail to connect kPreflightMaxAttempts times in a row,
    // falls into the on-device Bluetooth pairing/recovery screen
    // (obd_pairing::run()) instead of halting - see obd/obd_pairing.h.
    // Skipped entirely in OBD_SIMULATION_ENABLED builds: the scripted drive
    // cycle never touches these credentials, and requiring a real adapter's
    // mac/id/password just to boot the demo would defeat its "no adapter, no
    // vehicle, no Bluetooth pairing" purpose (see obd_simulator.h).
    ObdCredentials obdCredentials;
#if !OBD_SIMULATION_ENABLED
    bool needsPairing = !loadObdCredentials(sdManager, obdCredentials);
    if (!needsPairing && !obd_pairing::preflight(obdCredentials)) {
        Serial.println("[OBD] Stored credentials failed to connect; falling back to the pairing screen.");
        connection_log::write(sdManager, "Stored credentials failed preflight; entering pairing screen");
        needsPairing = true;
    }
    if (needsPairing) {
        const ThemeColors& pairingTheme = getTheme(static_cast<ThemeId>(configStore.settings().themeId));
        obd_pairing::run(displayManager, touchManager, sdManager, pairingTheme, obdCredentials);
    }
    connection_log::writef(sdManager, "Credentials loaded: id=%s", obdCredentials.id);
#endif

    // 6. Start the ELM327 Bluetooth client on its own task (using the
    // credentials validated/paired in step 5) early, before the boot screen
    // delay, so connection progress is visible during the boot logo display.
    obdClient.begin(obdCredentials, sdManager);

    // 7. Render Static Boot Screen (PNG decoded via PNGdec into RGB565)
    // and wait for OBD connection or timeout while showing status updates.
    Serial.println("[Boot] Displaying static boot image...");
    displayManager.drawBootImage();

    char lastBootStatus[64] = {0};
    uint32_t bootStartMs = millis();
    while (obdClient.getConnectionState() != ConnectionState::Live &&
           millis() - bootStartMs < config::kBootConnectTimeoutMs) {
        char statusMsg[64];
        obdClient.getLastStatusMessage(statusMsg, sizeof(statusMsg));
        if (statusMsg[0] != '\0' && strcmp(statusMsg, lastBootStatus) != 0) {
            strncpy(lastBootStatus, statusMsg, sizeof(lastBootStatus) - 1);
            lastBootStatus[sizeof(lastBootStatus) - 1] = '\0';
            displayManager.drawBootStatus(statusMsg);
        }
        delay(50);
    }
    if (millis() - bootStartMs < config::kBootScreenDurationMs) {
        delay(config::kBootScreenDurationMs - (millis() - bootStartMs)); // Keep the logo up for its original minimum branding time even on a fast connect
    }

    // 8. Start SD CSV logging (pruning old sessions first if needed). No-ops
    // safely when SD_LOGGING_ENABLED is unset or the card is missing.
#ifdef SD_LOGGING_ENABLED
    csvLogger.setUnitsMetric(configStore.settings().useMetricLogs);
    csvLogger.begin();
#else
    Serial.println("[Log] SD_LOGGING_ENABLED not defined; logging disabled.");
#endif

    // 9. Draw Page 1 chrome; drawDynamic() in loop() fills in live values.
    // drawStatic() below already repaints every pixel it touches (header +
    // full-screen background), so no pre-clear is needed here.
    TelemetrySnapshot initialSnapshot;
    obdClient.getSnapshot(initialSnapshot);
    clusterPages.drawStatic(touchHandler.currentPage(), initialSnapshot);
}

void loop() {
    uint32_t nowMs = millis();

    // Non-blocking touch polling; dispatch only on the touch-down edge so a
    // held finger doesn't repeat-fire navigation or button taps.
    if (nowMs - lastTouchPollMs >= config::kTouchPollIntervalMs) {
        lastTouchPollMs = nowMs;

        uint16_t touchX = 0;
        uint16_t touchY = 0;
        bool isTouched = touchManager.getTouch(touchX, touchY);

        if (isTouched && !lastTouchState) {
            ClusterPage previousPage = touchHandler.currentPage();
            bool pageChanged = touchHandler.handleTap(touchX, touchY, nowMs);
            if (pageChanged && touchHandler.currentPage() != previousPage) {
                TelemetrySnapshot snapshot;
                obdClient.getSnapshot(snapshot);
                // drawStatic() repaints every pixel of the new page (header +
                // full-screen background) itself; pre-clearing to black here
                // only injected a one-frame black flash on non-black themes.
                clusterPages.drawStatic(touchHandler.currentPage(), snapshot);
            }
        }
        lastTouchState = isTouched;
    }

    TelemetrySnapshot snapshot;
    obdClient.getSnapshot(snapshot);

    bool checkEngineOn = snapshot.milOn.valid && snapshot.milOn.value != 0.0F;
    const AppSettings& settings = configStore.settings();
    bool shiftLightOn = snapshot.rpm.valid && snapshot.rpm.value >= settings.shiftLightRpm;
    uint16_t warningColor = getTheme(static_cast<ThemeId>(settings.themeId)).warningActive;
    statusLed.update(checkEngineOn, shiftLightOn, warningColor, nowMs);

    clusterPages.updateBackgroundTelemetry(snapshot, nowMs);

#ifdef SD_LOGGING_ENABLED
    csvLogger.update(snapshot, nowMs, configStore.settings().logIntervalMs);
#endif

    if (nowMs - lastUiRefreshMs >= config::kUiRefreshIntervalMs) {
        lastUiRefreshMs = nowMs;
        ConnectionState connectionState = resolveConnectionState(snapshot, nowMs);
        clusterPages.drawDynamic(touchHandler.currentPage(), snapshot, connectionState, nowMs);
    }
}
