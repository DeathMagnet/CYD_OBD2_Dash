#include "display/touch_handler.h"
#include "display/cluster_layout.h"
#include "system/units.h"
#include <string.h>

ClusterTouchHandler::ClusterTouchHandler(ClusterPages& clusterPages, ConfigStore& configStore, CsvLogger& csvLogger,
                                          ObdClient& obdClient)
    : clusterPages_(clusterPages), configStore_(configStore), csvLogger_(csvLogger), obdClient_(obdClient) {}

bool ClusterTouchHandler::within(uint16_t x, uint16_t y, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    return static_cast<int32_t>(x) >= x0 && static_cast<int32_t>(x) < x1 && static_cast<int32_t>(y) >= y0 &&
           static_cast<int32_t>(y) < y1;
}

bool ClusterTouchHandler::handleTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    if (y < layout::kHeaderHeight) {
        return handleHeaderTap(x, y);
    }

    // The Save button footer is shared by every config page, so it's checked
    // once here rather than duplicated in each page's tap handler.
    if (isConfigPage(currentPage_) && y >= layout::kConfigFooterY) {
        return handleConfigFooterTap(x, y, nowMs);
    }

    switch (currentPage_) {
        case ClusterPage::ConfigUi: return handleConfigUiTap(x, y, nowMs);
        case ClusterPage::ConfigGauges: return handleConfigGaugesTap(x, y, nowMs);
        case ClusterPage::ConfigUserVars: return handleConfigUserVarsTap(x, y, nowMs);
        case ClusterPage::ConfigLogs: return handleConfigLogsTap(x, y, nowMs);
        case ClusterPage::Diagnostics: return handleDiagnosticsTap(x, y, nowMs);
        case ClusterPage::PerformanceTelemetry: return handlePerformanceTap(x, y);
        default: return false;
    }
}

bool ClusterTouchHandler::handleHeaderTap(uint16_t x, uint16_t y) {
    if (within(x, y, layout::kNavPrevX0, 0, layout::kNavPrevX1, layout::kHeaderHeight)) {
        uint8_t groupStart = isConfigPage(currentPage_) ? static_cast<uint8_t>(kFirstConfigPage) : 0;
        uint8_t groupEnd =
            isConfigPage(currentPage_) ? static_cast<uint8_t>(ClusterPage::Count) : static_cast<uint8_t>(kFirstConfigPage);
        uint8_t groupSize = groupEnd - groupStart;
        uint8_t rel = static_cast<uint8_t>(currentPage_) - groupStart;
        rel = (rel == 0) ? groupSize - 1 : rel - 1;
        currentPage_ = static_cast<ClusterPage>(groupStart + rel);
        return true;
    }
    if (within(x, y, layout::kNavNextX0, 0, layout::kNavNextX1, layout::kHeaderHeight)) {
        uint8_t groupStart = isConfigPage(currentPage_) ? static_cast<uint8_t>(kFirstConfigPage) : 0;
        uint8_t groupEnd =
            isConfigPage(currentPage_) ? static_cast<uint8_t>(ClusterPage::Count) : static_cast<uint8_t>(kFirstConfigPage);
        uint8_t groupSize = groupEnd - groupStart;
        uint8_t rel = static_cast<uint8_t>(currentPage_) - groupStart;
        rel = (rel + 1) % groupSize;
        currentPage_ = static_cast<ClusterPage>(groupStart + rel);
        return true;
    }
    if (within(x, y, layout::kMilZoneX0, 0, layout::kMilZoneX1, layout::kHeaderHeight)) {
        // MIL icon is only tappable when MIL is active
        TelemetrySnapshot snapshot;
        obdClient_.getSnapshot(snapshot);
        bool milOn = snapshot.milOn.valid && snapshot.milOn.value != 0.0F;
        if (milOn) {
            currentPage_ = ClusterPage::Diagnostics;
            return true;
        }
    }
    if (within(x, y, layout::kModeToggleX0, 0, layout::kModeToggleX1, layout::kHeaderHeight)) {
        if (isConfigPage(currentPage_)) {
            lastConfigPage_ = currentPage_;
            currentPage_ = lastDashboardPage_;
        } else {
            lastDashboardPage_ = currentPage_;
            currentPage_ = lastConfigPage_;
        }
        return true;
    }
    return false;
}

bool ClusterTouchHandler::handleConfigUiTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();

    // Active theme cycle row (row 0). Mustang S197 isn't implemented yet, so
    // this only toggles between the two implemented themes.
    int32_t row0 = layout::kConfigRow0Y + layout::kConfigButtonInsetY;
    int32_t row0End = row0 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row0, layout::kConfigCycleX + layout::kConfigCycleW, row0End)) {
        ThemeId newTheme = (settings.themeId == static_cast<uint8_t>(ThemeId::ModernFlat)) ? ThemeId::TorqueNeon
                                                                                            : ThemeId::ModernFlat;
        configStore_.setThemeId(static_cast<uint8_t>(newTheme));
        clusterPages_.applyTheme(newTheme);
        TelemetrySnapshot snapshot;
        obdClient_.getSnapshot(snapshot);
        clusterPages_.drawStatic(ClusterPage::ConfigUi, snapshot);
        return false;
    }

    // Units toggle row (row 1)
    int32_t row1 = layout::kConfigRow1Y + layout::kConfigButtonInsetY;
    int32_t row1End = row1 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row1, layout::kConfigCycleX + layout::kConfigCycleW, row1End)) {
        configStore_.setUseMetricUnits(!settings.useMetricUnits);
        return false;
    }

    return false;
}

bool ClusterTouchHandler::handleConfigUserVarsTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;

    int32_t row0 = layout::kConfigRow0Y + layout::kConfigButtonInsetY;
    int32_t row0End = row0 + layout::kConfigButtonH;
    float step = config::kBaroBaselineStepPsi;
    if (metric) {
        step = units::psiFromKpa(config::kBaroBaselineStepKpa);
    }
    if (within(x, y, layout::kConfigMinusX, row0, layout::kConfigMinusX + layout::kConfigMinusW, row0End)) {
        configStore_.setBaroBaselinePsi(settings.baroBaselinePsi - step);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row0, layout::kConfigPlusX + layout::kConfigPlusW, row0End)) {
        configStore_.setBaroBaselinePsi(settings.baroBaselinePsi + step);
        return false;
    }

    return false;
}

bool ClusterTouchHandler::handleConfigGaugesTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;

    int32_t row0 = layout::kConfigRow0Y + layout::kConfigButtonInsetY;
    int32_t row0End = row0 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row0, layout::kConfigMinusX + layout::kConfigMinusW, row0End)) {
        configStore_.setShiftLightRpm(settings.shiftLightRpm - config::kShiftLightStepRpm);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row0, layout::kConfigPlusX + layout::kConfigPlusW, row0End)) {
        configStore_.setShiftLightRpm(settings.shiftLightRpm + config::kShiftLightStepRpm);
        return false;
    }

    int32_t row1 = layout::kConfigRow1Y + layout::kConfigButtonInsetY;
    int32_t row1End = row1 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row1, layout::kConfigMinusX + layout::kConfigMinusW, row1End)) {
        configStore_.setRedlineRpm(settings.redlineRpm - config::kRedlineStepRpm);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row1, layout::kConfigPlusX + layout::kConfigPlusW, row1End)) {
        configStore_.setRedlineRpm(settings.redlineRpm + config::kRedlineStepRpm);
        return false;
    }

    int32_t row2 = layout::kConfigRow2Y + layout::kConfigButtonInsetY;
    int32_t row2End = row2 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row2, layout::kConfigMinusX + layout::kConfigMinusW, row2End)) {
        configStore_.setMaxRpm(settings.maxRpm - config::kMaxRpmStepRpm);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row2, layout::kConfigPlusX + layout::kConfigPlusW, row2End)) {
        configStore_.setMaxRpm(settings.maxRpm + config::kMaxRpmStepRpm);
        return false;
    }

    int32_t row3 = layout::kConfigRow3Y + layout::kConfigButtonInsetY;
    int32_t row3End = row3 + layout::kConfigButtonH;
    uint16_t step = config::kMaxSpeedStepMph;
    if (metric) {
        step = static_cast<uint16_t>(units::mphFromKph(config::kMaxSpeedStepKph));
    }
    if (within(x, y, layout::kConfigMinusX, row3, layout::kConfigMinusX + layout::kConfigMinusW, row3End)) {
        configStore_.setMaxSpeedMph(settings.maxSpeedMph - step);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row3, layout::kConfigPlusX + layout::kConfigPlusW, row3End)) {
        configStore_.setMaxSpeedMph(settings.maxSpeedMph + step);
        return false;
    }

    return false;
}

bool ClusterTouchHandler::handleConfigLogsTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    ClusterPageRuntimeState& state = clusterPages_.runtimeState();
    const AppSettings& settings = configStore_.settings();

    int32_t row0 = layout::kConfigRow0Y + layout::kConfigButtonInsetY;
    int32_t row0End = row0 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row0, layout::kConfigCycleX + layout::kConfigCycleW, row0End)) {
        uint32_t current = settings.logIntervalMs;
        size_t idx = 0;
        for (size_t i = 0; i < config::kLogRowIntervalOptionCount; ++i) {
            if (config::kLogRowIntervalOptionsMs[i] == current) {
                idx = i;
                break;
            }
        }
        idx = (idx + 1) % config::kLogRowIntervalOptionCount;
        configStore_.setLogIntervalMs(config::kLogRowIntervalOptionsMs[idx]);
        return false;
    }

    // Log Units row (row 1)
    int32_t row1 = layout::kConfigRow1Y + layout::kConfigButtonInsetY;
    int32_t row1End = row1 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row1, layout::kConfigCycleX + layout::kConfigCycleW, row1End)) {
        configStore_.setUseMetricLogs(!settings.useMetricLogs);
        return false;
    }

    int32_t summaryRow = layout::kLogsSummaryRowY + layout::kConfigButtonInsetY;
    int32_t summaryRowEnd = summaryRow + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigDeleteX, summaryRow, layout::kConfigDeleteX + layout::kConfigDeleteW,
               summaryRowEnd)) {
        if (state.deleteLogsConfirmArmed && (nowMs - state.deleteLogsConfirmArmedAtMs < 5000)) {
            csvLogger_.deleteAllLogs();
            state.deleteLogsConfirmArmed = false;
            state.cfgLogSummaryNextScanMs = 0; // Force an immediate rescan to reflect the deletion.
        } else {
            state.deleteLogsConfirmArmed = true;
            state.deleteLogsConfirmArmedAtMs = nowMs;
        }
        return false;
    }

    return false;
}

bool ClusterTouchHandler::handleConfigFooterTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    ClusterPageRuntimeState& state = clusterPages_.runtimeState();

    int32_t footerY = layout::kConfigFooterY + layout::kConfigButtonInsetY;
    int32_t footerYEnd = footerY + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigSaveX, footerY, layout::kConfigSaveX + layout::kConfigSaveW, footerYEnd)) {
        bool logUnitsChanged = configStore_.isLogUnitsDirty();
        bool newMetricLogs = configStore_.settings().useMetricLogs;

        configStore_.save();

        if (logUnitsChanged) {
            csvLogger_.setUnitsMetric(newMetricLogs);
            csvLogger_.deleteAllLogs();
            state.cfgLogSummaryNextScanMs = 0; // force the LOGS/MB summary to rescan and show the wipe immediately
        }

        strncpy(state.configStatusMessage, "SAVED", sizeof(state.configStatusMessage) - 1);
        state.configStatusMessage[sizeof(state.configStatusMessage) - 1] = '\0';
        state.configStatusMessageSetAtMs = nowMs;
        return false;
    }

    return false;
}

bool ClusterTouchHandler::handleDiagnosticsTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    ClusterPageRuntimeState& state = clusterPages_.runtimeState();

    if (within(x, y, layout::kDtcReadButtonX, layout::kDtcButtonY, layout::kDtcReadButtonX + layout::kDtcReadButtonW,
                layout::kDtcButtonY + layout::kDtcButtonH)) {
        obdClient_.requestDtcRead();
        return false;
    }

    if (within(x, y, layout::kDtcClearButtonX, layout::kDtcButtonY,
                layout::kDtcClearButtonX + layout::kDtcClearButtonW, layout::kDtcButtonY + layout::kDtcButtonH)) {
        if (state.clearCodesConfirmArmed && (nowMs - state.clearCodesConfirmArmedAtMs < 5000)) {
            obdClient_.requestClearCodes();
            state.clearCodesConfirmArmed = false;
        } else {
            state.clearCodesConfirmArmed = true;
            state.clearCodesConfirmArmedAtMs = nowMs;
        }
        return false;
    }

    return false;
}

bool ClusterTouchHandler::handlePerformanceTap(uint16_t x, uint16_t y) {
    if (within(x, y, layout::kPerfTimerX0, layout::kPerfTimerY0, layout::kPerfTimerX1, layout::kPerfTimerY1)) {
        ClusterPageRuntimeState& state = clusterPages_.runtimeState();
        state.zeroToSixtyRunning = false;
        state.zeroToSixtyResultSec = -1.0F;
        state.lastSpeedMphForTimer = 0.0F;
    }
    return false;
}
