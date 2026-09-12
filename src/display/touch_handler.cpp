#include "display/touch_handler.h"
#include "display/cluster_layout.h"
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

    switch (currentPage_) {
        case ClusterPage::ConfigMenu: return handleConfigMenuTap(x, y, nowMs);
        case ClusterPage::Diagnostics: return handleDiagnosticsTap(x, y, nowMs);
        case ClusterPage::PerformanceTelemetry: return handlePerformanceTap(x, y);
        default: return false;
    }
}

bool ClusterTouchHandler::handleHeaderTap(uint16_t x, uint16_t y) {
    if (within(x, y, layout::kNavPrevX0, 0, layout::kNavPrevX1, layout::kHeaderHeight)) {
        uint8_t idx = static_cast<uint8_t>(currentPage_);
        idx = (idx == 0) ? static_cast<uint8_t>(ClusterPage::Count) - 1 : idx - 1;
        currentPage_ = static_cast<ClusterPage>(idx);
        return true;
    }
    if (within(x, y, layout::kNavNextX0, 0, layout::kNavNextX1, layout::kHeaderHeight)) {
        uint8_t idx = static_cast<uint8_t>((static_cast<uint8_t>(currentPage_) + 1) %
                                            static_cast<uint8_t>(ClusterPage::Count));
        currentPage_ = static_cast<ClusterPage>(idx);
        return true;
    }
    if (within(x, y, layout::kMilZoneX0, 0, layout::kMilZoneX1, layout::kHeaderHeight)) {
        currentPage_ = ClusterPage::Diagnostics;
        return true;
    }
    if (within(x, y, layout::kNavTitleX0, 0, layout::kNavTitleX1, layout::kHeaderHeight)) {
        currentPage_ = ClusterPage::ConfigMenu;
        return true;
    }
    return false;
}

bool ClusterTouchHandler::handleConfigMenuTap(uint16_t x, uint16_t y, uint32_t nowMs) {
    ClusterPageRuntimeState& state = clusterPages_.runtimeState();
    const AppSettings& settings = configStore_.settings();

    int32_t row1 = layout::kConfigRow1Y + layout::kConfigButtonInsetY;
    int32_t row1End = row1 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row1, layout::kConfigMinusX + layout::kConfigMinusW, row1End)) {
        configStore_.setShiftLightRpm(settings.shiftLightRpm - config::kShiftLightStepRpm);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row1, layout::kConfigPlusX + layout::kConfigPlusW, row1End)) {
        configStore_.setShiftLightRpm(settings.shiftLightRpm + config::kShiftLightStepRpm);
        return false;
    }

    int32_t row2 = layout::kConfigRow2Y + layout::kConfigButtonInsetY;
    int32_t row2End = row2 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row2, layout::kConfigMinusX + layout::kConfigMinusW, row2End)) {
        configStore_.setRedlineRpm(settings.redlineRpm - config::kRedlineStepRpm);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row2, layout::kConfigPlusX + layout::kConfigPlusW, row2End)) {
        configStore_.setRedlineRpm(settings.redlineRpm + config::kRedlineStepRpm);
        return false;
    }

    int32_t row3 = layout::kConfigRow3Y + layout::kConfigButtonInsetY;
    int32_t row3End = row3 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row3, layout::kConfigCycleX + layout::kConfigCycleW, row3End)) {
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

    int32_t row4 = layout::kConfigRow4Y + layout::kConfigButtonInsetY;
    int32_t row4End = row4 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row4, layout::kConfigMinusX + layout::kConfigMinusW, row4End)) {
        configStore_.setBaroBaselinePsi(settings.baroBaselinePsi - config::kBaroBaselineStepPsi);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row4, layout::kConfigPlusX + layout::kConfigPlusW, row4End)) {
        configStore_.setBaroBaselinePsi(settings.baroBaselinePsi + config::kBaroBaselineStepPsi);
        return false;
    }

    int32_t row5 = layout::kConfigRow5Y + layout::kConfigButtonInsetY;
    int32_t row5End = row5 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigSaveX, row5, layout::kConfigSaveX + layout::kConfigSaveW, row5End)) {
        configStore_.save();
        strncpy(state.configStatusMessage, "SAVED", sizeof(state.configStatusMessage) - 1);
        state.configStatusMessage[sizeof(state.configStatusMessage) - 1] = '\0';
        state.configStatusMessageSetAtMs = nowMs;
        return false;
    }

    int32_t row6 = layout::kConfigRow6Y + layout::kConfigButtonInsetY;
    int32_t row6End = row6 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigDeleteX, row6, layout::kConfigDeleteX + layout::kConfigDeleteW, row6End)) {
        if (state.deleteLogsConfirmArmed && (nowMs - state.deleteLogsConfirmArmedAtMs < 5000)) {
            csvLogger_.deleteAllLogs();
            state.deleteLogsConfirmArmed = false;
        } else {
            state.deleteLogsConfirmArmed = true;
            state.deleteLogsConfirmArmedAtMs = nowMs;
        }
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
