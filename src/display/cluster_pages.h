#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>

#include "display/theme.h"
#include "display/gauge_widgets.h"
#include "obd/telemetry.h"
#include "obd/obd_client.h"
#include "system/connection_state.h"
#include "system/config_store.h"
#include "logging/csv_logger.h"

enum class ClusterPage : uint8_t {
    PrimaryCluster = 0,   // Page 1
    EngineLoadAirflow,    // Page 2
    CarSpecificSensors,   // Page 3
    PerformanceTelemetry, // Page 4
    ConfigMenu,           // Page 5
    Diagnostics,          // Page 6
    Count
};

// Transient UI state that isn't a persisted setting and isn't part of the
// telemetry snapshot: timers, rolling history, and short-lived confirmation
// prompts. Owned by ClusterPages so touch_handler.cpp can reach into it when
// handling taps (e.g. arming a delete/clear confirmation).
struct ClusterPageRuntimeState {
    // Page 4 - 0-60 MPH timer.
    bool zeroToSixtyRunning = false;
    uint32_t zeroToSixtyStartMs = 0;
    float zeroToSixtyResultSec = -1.0F; // -1 = no result yet
    float lastSpeedMphForTimer = 0.0F;

    // Page 4 - rolling MAF history for the intake airflow graph. Stored
    // oldest-first and shifted (not a modular ring buffer) since one sample
    // per second makes the O(n) shift negligible and keeps rendering simple.
    static constexpr uint8_t kMafHistorySize = 60;
    float mafHistory[kMafHistorySize] = {0};
    uint8_t mafHistoryCount = 0;
    uint32_t lastMafSampleMs = 0;
    float mafPeakGps = 0.0F;

    // Page 6 - DTC clear confirmation and status feedback.
    bool clearCodesConfirmArmed = false;
    uint32_t clearCodesConfirmArmedAtMs = 0;
    char statusMessage[48] = {0};
    uint32_t statusMessageSetAtMs = 0;

    // Page 5 - delete-all-logs confirmation and save feedback.
    bool deleteLogsConfirmArmed = false;
    uint32_t deleteLogsConfirmArmedAtMs = 0;
    char configStatusMessage[32] = {0};
    uint32_t configStatusMessageSetAtMs = 0;
};

class ClusterPages {
public:
    ClusterPages(TFT_eSPI& tft, ConfigStore& configStore, CsvLogger& csvLogger, ObdClient& obdClient);

    // Full chrome redraw for `page`: header, nav zones, page-specific static
    // layout. Call once whenever the active page changes.
    void drawStatic(ClusterPage page, const TelemetrySnapshot& snapshot);

    // Redraws only the values that change frame to frame. Call at a
    // throttled UI cadence (see main.cpp; not every loop() iteration).
    void drawDynamic(ClusterPage page, const TelemetrySnapshot& snapshot,
                      ConnectionState connectionState, bool sdLoggingActive, uint32_t nowMs);

    // Samples telemetry for the 0-60 timer and MAF rolling graph every tick
    // regardless of which page is visible, matching the doc's "automatic
    // start" behavior for the timer.
    void updateBackgroundTelemetry(const TelemetrySnapshot& snapshot, uint32_t nowMs);

    ClusterPageRuntimeState& runtimeState() { return runtimeState_; }

private:
    void drawHeader(ClusterPage page, bool milOn);
    void drawStatusStrip(ConnectionState connectionState, bool sdLoggingActive);

    void drawPage1Static();
    void drawPage1Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage2Static();
    void drawPage2Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage3Static();
    void drawPage3Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage4Static();
    void drawPage4Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage5Static();
    void drawPage5Dynamic(uint32_t nowMs);
    void drawPage6Static();
    void drawPage6Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);

    TFT_eSPI& tft_;
    ConfigStore& configStore_;
    CsvLogger& csvLogger_;
    ObdClient& obdClient_;
    const ThemeColors& theme_;
    ClusterPageRuntimeState runtimeState_;
    NeedlePhysics rpmNeedle_;
    NeedlePhysics speedNeedle_;
    uint32_t lastFrameMs_ = 0;
};
