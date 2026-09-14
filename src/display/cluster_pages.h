#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>

#include "display/theme.h"
#include "display/gauge_widgets.h"
#include "obd/telemetry.h"
#include "obd/obd_client.h"
#include "system/connection_state.h"
#include "system/config_store.h"
#include "system/dtc_decoder.h"
#include "logging/csv_logger.h"

// Dashboard and config-menu pages are kept as contiguous enum ranges (rather
// than in on-screen page-number order) so group membership and group-cycling
// (see ClusterTouchHandler::handleHeaderTap) reduce to a simple index range
// check instead of a per-page lookup table.
enum class ClusterPage : uint8_t {
    PrimaryCluster = 0,   // Page 1 - dashboard
    EngineLoadAirflow,    // Page 2 - dashboard
    CarSpecificSensors,   // Page 3 - dashboard
    PerformanceTelemetry, // Page 4 - dashboard
    Diagnostics,          // Page 5 - dashboard (last in group)
    ConfigUi,             // first config-group page ("UI")
    ConfigGauges,         // config-group page ("GAUGES")
    ConfigUserVars,       // config-group page ("USER VARS")
    ConfigLogs,           // config-group page ("LOGS")
    ConfigObd,            // config-group page ("OBD ADAPTER")
    Count
};

constexpr ClusterPage kFirstConfigPage = ClusterPage::ConfigUi;

constexpr bool isConfigPage(ClusterPage page) {
    return page >= kFirstConfigPage;
}

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

    // Page 5 - DTC clear confirmation and status feedback.
    bool clearCodesConfirmArmed = false;
    uint32_t clearCodesConfirmArmedAtMs = 0;
    char statusMessage[48] = {0};
    uint32_t statusMessageSetAtMs = 0;

    // Page 5 - last-drawn state, so drawPage5Dynamic() only repaints regions
    // whose content actually changed instead of every UI refresh tick.
    // -1 means "not drawn yet" and forces a redraw the first time; reset
    // whenever drawPage5Static() reopens the page.
    int8_t dtcMilOnDrawn = -1;
    int8_t dtcHaveResultDrawn = -1;
    DtcList dtcListDrawn;
    int8_t dtcClearConfirmDrawn = -1;

    // Header MIL icon - last-drawn state, so drawDynamic() only repaints the
    // icon when milOn actually changes instead of every UI refresh tick.
    // -1 means "not drawn yet" and forces a redraw the first time.
    int8_t headerMilOnDrawn = -1;

    // Header connection status badge - last-drawn state, so drawDynamic()
    // only repaints the badge when connectionState actually changes instead
    // of every UI refresh tick. -1 means "not drawn yet" and forces a
    // redraw the first time.
    int8_t statusStripDrawn = -1;

    // Page 1 - last-drawn "lit tick count" for each gauge's tick marks, so
    // drawPage1Dynamic() only repaints ticks when the needle has actually
    // crossed into/out of a new tick instead of every UI refresh tick. -1
    // means "not drawn yet" and forces a redraw the first time.
    int32_t rpmTickLitCountDrawn = -1;
    int32_t speedTickLitCountDrawn = -1;

    // Page 4 - last-drawn sample timestamp for the MAF history graph, so
    // drawPage4Dynamic() only redraws it when a new sample has actually been
    // recorded (once per second) instead of every UI refresh tick. Sentinel
    // (impossible millis() value the real timestamp can't equal at boot)
    // forces a redraw the first time.
    uint32_t mafGraphDrawnAtSampleMs = 0xFFFFFFFFu;

    // Logs page - delete-all-logs confirmation.
    bool deleteLogsConfirmArmed = false;
    uint32_t deleteLogsConfirmArmedAtMs = 0;

    // Shared config footer (Save button) - feedback message/timeout, shown on
    // whichever config page is active when Save is tapped.
    char configStatusMessage[32] = {0};
    uint32_t configStatusMessageSetAtMs = 0;

    // UI/Logs pages - last-drawn field text/button state, so
    // drawConfigUiDynamic()/drawConfigLogsDynamic() only repaint a field when
    // its value actually changed instead of every UI refresh tick. Empty
    // string / -1 means "not drawn yet" and forces a redraw the first time;
    // reset whenever the owning page's Static function reopens the page.
    char cfgShiftLightRpmDrawn[24] = {0};  // Gauges page
    char cfgRedlineRpmDrawn[24] = {0};     // Gauges page
    char cfgMaxRpmDrawn[24] = {0};         // Gauges page
    char cfgMaxSpeedDrawn[24] = {0};       // Gauges page
    char cfgBaroBaselineDrawn[24] = {0};   // User Vars page
    char cfgCoolantWarningDrawn[24] = {0};    // User Vars page
    char cfgLowVoltageWarningDrawn[24] = {0}; // User Vars page
    char cfgBoostMaxDrawn[24] = {0};          // Gauges page
    char cfgVacuumMaxDrawn[24] = {0};         // Gauges page
    char cfgZeroSixtyTargetDrawn[24] = {0};   // User Vars page
    char cfgHpFactorDrawn[24] = {0};          // User Vars page
    char cfgFuelTrimRangeDrawn[24] = {0};     // User Vars page
    char cfgUnitsDrawn[32] = {0};          // UI page
    char cfgThemeDrawn[24] = {0};          // UI page
    char cfgTicksDrawn[32] = {0};          // UI page
    char cfgLogIntervalDrawn[24] = {0};    // Logs page
    char cfgLogUnitsDrawn[32] = {0};       // Logs page
    int8_t cfgDeleteConfirmDrawn = -1;     // Logs page
    char cfgObdAdapterNameDrawn[24] = {0}; // OBD Adapter page
    char cfgObdAdapterPinDrawn[16] = {0};  // OBD Adapter page

    // Shared config footer (Save button) - same "not drawn yet" convention as
    // above, reset whenever any config page's Static function reopens.
    int8_t cfgSaveButtonDrawn = -1;

    // Logs page - log summary (file count/size) is expensive to compute (it
    // scans the SD card directory), so it's only rescanned periodically
    // rather than on every UI refresh tick. Setting cfgLogSummaryNextScanMs
    // to 0 forces an immediate rescan on the next drawConfigLogsDynamic() call.
    char cfgLogSummaryDrawn[32] = {0};
    uint32_t cfgLogSummaryNextScanMs = 0;
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
                      ConnectionState connectionState, uint32_t nowMs);

    // Samples telemetry for the 0-60 timer and MAF rolling graph every tick
    // regardless of which page is visible, matching the doc's "automatic
    // start" behavior for the timer.
    void updateBackgroundTelemetry(const TelemetrySnapshot& snapshot, uint32_t nowMs);

    ClusterPageRuntimeState& runtimeState() { return runtimeState_; }

    // Switches the active color theme. Callers must repaint (drawStatic) the
    // visible page afterward to see the change; this only updates theme_.
    void applyTheme(ThemeId id) { theme_ = getTheme(id); }

private:
    void drawHeader(ClusterPage page, bool milOn);
    void drawStatusStrip(ConnectionState connectionState);

    // Applies the given "large text" size for a readout that would otherwise use
    // setTextSize(3+) on the default font. Under a theme with useSevenSegmentFont,
    // swaps to TFT_eSPI's built-in Font 7 (7-segment LED look; digits + ':' '.' '-'
    // only) at its native size instead of scaling the default font. Always pair
    // with endLargeText() before drawing any other text, since font selection is
    // sticky in TFT_eSPI.
    void beginLargeText(uint8_t size);
    void endLargeText();

    // Page 2 - vertical layout for the MAF + TIMING ADVANCE box. Computed from
    // the active theme's value-font height (via applyValueFont/fontHeight) so
    // the 3 MAF lines (label/value/peak) stay snug at the top, the 2 TIMING
    // ADVANCE lines (label/value) stay snug at the bottom, and the box is
    // sized to fit both groups with consistent padding in every theme.
    struct MafTimingLayout {
        int32_t mafLabelY;
        int32_t mafLabelToValueGap;
        int32_t peakY;
        int32_t taLabelY;
        int32_t taLabelToValueGap;
        int32_t boxHeight;
    };
    MafTimingLayout computeMafTimingLayout();

    // Page 3 - vertical layout for the Vacuum/Boost panel: label (with its
    // unit folded in, since the value's font may not support letters) on top,
    // the bar gauge below it, and the value centered beneath the bar. Sized
    // from the active theme's value-font height so the box fits all 3 lines
    // snugly in every theme.
    struct VacuumLayout {
        int32_t labelY;
        int32_t barY;
        int32_t valueY;
        int32_t boxHeight;
    };
    VacuumLayout computeVacuumLayout();

    void drawPage1Static();
    void drawPage1Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage2Static();
    void drawPage2Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage3Static();
    void drawPage3Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawPage4Static();
    void drawPage4Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);
    void drawConfigUiStatic();
    void drawConfigUiDynamic(uint32_t nowMs);
    void drawConfigGaugesStatic();
    void drawConfigGaugesDynamic(uint32_t nowMs);
    void drawConfigUserVarsStatic();
    void drawConfigUserVarsDynamic(uint32_t nowMs);
    void drawConfigLogsStatic();
    void drawConfigLogsDynamic(uint32_t nowMs);
    void drawConfigObdStatic();
    void drawConfigObdDynamic(uint32_t nowMs);
    void drawConfigFooterStatic();
    void drawConfigFooterDynamic(uint32_t nowMs);
    void drawPage5Static();
    void drawPage5Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs);

    TFT_eSPI& tft_;
    ConfigStore& configStore_;
    CsvLogger& csvLogger_;
    ObdClient& obdClient_;
    ThemeColors theme_;
    ClusterPageRuntimeState runtimeState_;
    NeedlePhysics rpmNeedle_;
    NeedlePhysics speedNeedle_;
    gaugewidgets::ArcGaugeState rpmArc_;
    gaugewidgets::ArcGaugeState speedArc_;
    gaugewidgets::ArcGaugeState loadArc_;
    gaugewidgets::BarGaugeState vacuumBar_;
    gaugewidgets::BarGaugeState throttleBar_;
    gaugewidgets::BarGaugeState stftBar_;
    gaugewidgets::BarGaugeState ltftBar_;
    uint32_t lastFrameMs_ = 0;
};
