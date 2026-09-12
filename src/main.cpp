#include <Arduino.h>
#include "app_config.h"
#include "display/display_manager.h"
#include "storage/sd_manager.h"
#include "input/touch_manager.h"
#include "system/config_store.h"
#include "system/connection_state.h"
#include "logging/csv_logger.h"
#include "obd/obd_client.h"
#include "obd/telemetry.h"
#include "display/cluster_pages.h"
#include "display/touch_handler.h"

// Subsystem instances
static DisplayManager displayManager;
static SdManager sdManager;
static TouchManager touchManager(displayManager.getTft(), sdManager);
static ConfigStore configStore(sdManager);
static CsvLogger csvLogger(sdManager);
static ObdClient obdClient;
static ClusterPages clusterPages(displayManager.getTft(), configStore, csvLogger, obdClient);
static ClusterTouchHandler touchHandler(clusterPages, configStore, csvLogger, obdClient);

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

    // 1. Initialize Display & Backlight
    displayManager.begin();

    // 2. Render Static Boot Screen from native RGB565 PROGMEM array
    Serial.println("[Boot] Displaying static boot image...");
    displayManager.drawBootImage();
    delay(config::kBootScreenDurationMs);

    // 3. Initialize SD Card over VSPI
    sdManager.begin();

    // 4. Initialize Touch & Run Calibration if missing from SD
    touchManager.begin();

    // 5. Load persisted settings (Page 5), falling back to defaults if the
    // SD card or /config.txt is unavailable.
    configStore.begin();

    // 6. Start SD CSV logging (pruning old sessions first if needed). No-ops
    // safely when SD_LOGGING_ENABLED is unset or the card is missing.
#ifdef SD_LOGGING_ENABLED
    csvLogger.begin();
#else
    Serial.println("[Log] SD_LOGGING_ENABLED not defined; logging disabled.");
#endif

    // 7. Start the ELM327 Bluetooth client on its own task so connecting/
    // polling never blocks the render loop below.
    obdClient.begin();

    // 8. Draw Page 1 chrome; drawDynamic() in loop() fills in live values.
    displayManager.clear(TFT_BLACK);
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
                displayManager.clear(TFT_BLACK);
                clusterPages.drawStatic(touchHandler.currentPage(), snapshot);
            }
        }
        lastTouchState = isTouched;
    }

    TelemetrySnapshot snapshot;
    obdClient.getSnapshot(snapshot);

    clusterPages.updateBackgroundTelemetry(snapshot, nowMs);

#ifdef SD_LOGGING_ENABLED
    csvLogger.update(snapshot, nowMs, configStore.settings().logIntervalMs);
#endif

    if (nowMs - lastUiRefreshMs >= config::kUiRefreshIntervalMs) {
        lastUiRefreshMs = nowMs;
        ConnectionState connectionState = resolveConnectionState(snapshot, nowMs);
        clusterPages.drawDynamic(touchHandler.currentPage(), snapshot, connectionState, csvLogger.isLoggingActive(),
                                  nowMs);
    }
}
