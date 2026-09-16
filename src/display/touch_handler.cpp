#include "display/touch_handler.h"
#include "display/cluster_layout.h"
#include "system/units.h"
#include <Arduino.h>
#include <string.h>
#include <cmath>

ClusterTouchHandler::ClusterTouchHandler(ClusterPages& clusterPages, ConfigStore& configStore, CsvLogger& csvLogger,
                                          ObdClient& obdClient, SdManager& sdManager)
    : clusterPages_(clusterPages), configStore_(configStore), csvLogger_(csvLogger), obdClient_(obdClient),
      sdManager_(sdManager) {}

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
    const AppSettings& settings = configStore_.settings();

    // Active theme cycle row (row 0): cycles through all four themes in
    // order, Modern Flat -> Neon -> S197 - Digital -> S197 - Analog -> Modern Flat.
    int32_t row0 = layout::kConfigRow0Y + layout::kConfigButtonInsetY;
    int32_t row0End = row0 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row0, layout::kConfigCycleX + layout::kConfigCycleW, row0End)) {
        ThemeId newTheme;
        switch (static_cast<ThemeId>(settings.themeId)) {
            case ThemeId::ModernFlat: newTheme = ThemeId::Neon; break;
            case ThemeId::Neon: newTheme = ThemeId::S197Digital; break;
            case ThemeId::S197Digital: newTheme = ThemeId::S197Analog; break;
            case ThemeId::S197Analog:
            default: newTheme = ThemeId::ModernFlat; break;
        }
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

    // Gauge tick mode cycle row (row 2): cycles Off -> Inside -> Outside ->
    // Inside+Outside -> Off, except either S197 variant (no outside-tick art)
    // which only toggles Off <-> Inside.
    int32_t row2 = layout::kConfigRow2Y + layout::kConfigButtonInsetY;
    int32_t row2End = row2 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row2, layout::kConfigCycleX + layout::kConfigCycleW, row2End)) {
        ThemeId activeTheme = static_cast<ThemeId>(settings.themeId);
        TickMode current = static_cast<TickMode>(settings.tickMode);
        TickMode next;
        if (activeTheme == ThemeId::S197Digital || activeTheme == ThemeId::S197Analog) {
            next = (current == TickMode::Off) ? TickMode::InsideOnly : TickMode::Off;
        } else {
            switch (current) {
                case TickMode::Off: next = TickMode::InsideOnly; break;
                case TickMode::InsideOnly: next = TickMode::OutsideOnly; break;
                case TickMode::OutsideOnly: next = TickMode::InsideAndOutside; break;
                case TickMode::InsideAndOutside:
                default: next = TickMode::Off; break;
            }
        }
        configStore_.setTickMode(static_cast<uint8_t>(next));
        return false;
    }

    // Touch calibration row (row 3): tap-to-confirm, same pattern as Delete
    // All Logs. Acts immediately on confirm rather than staging until the
    // page's Save button is tapped, since there's no persisted setting here.
    int32_t row3 = layout::kConfigRow3Y + layout::kConfigButtonInsetY;
    int32_t row3End = row3 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigActionX, row3, layout::kConfigActionX + layout::kConfigActionW, row3End)) {
        ClusterPageRuntimeState& state = clusterPages_.runtimeState();
        if (state.recalibrateTouchConfirmArmed && (nowMs - state.recalibrateTouchConfirmArmedAtMs < 5000)) {
            sdManager_.deleteTouchCalibration();
            // Same "show feedback, then restart" pattern as the Flip Screen
            // save path below: without this, the screen goes instantly black
            // and straight into the corner-tap calibration prompt, which is
            // easy to mistake for a hang and invites a startled, rushed
            // recalibration.
            strncpy(state.configStatusMessage, "RECALIBRATING - RESTARTING", sizeof(state.configStatusMessage) - 1);
            state.configStatusMessage[sizeof(state.configStatusMessage) - 1] = '\0';
            state.configStatusMessageSetAtMs = nowMs;
            clusterPages_.drawSavedFeedback(nowMs);
            delay(1200);
            ESP.restart();
        } else {
            state.recalibrateTouchConfirmArmed = true;
            state.recalibrateTouchConfirmArmedAtMs = nowMs;
        }
        return false;
    }

    // Flip screen toggle row (row 4): stages the change only. The actual
    // rotation swap + touch recalibration happens on Save (see
    // handleConfigFooterTap), matching the Log Units page's stage-until-save
    // pattern.
    int32_t row4 = layout::kConfigRow4Y + layout::kConfigButtonInsetY;
    int32_t row4End = row4 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigCycleX, row4, layout::kConfigCycleX + layout::kConfigCycleW, row4End)) {
        configStore_.setScreenFlipped(!settings.screenFlipped);
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
    float baselineStep = config::kBaroBaselineStepPsi;
    if (metric) {
        baselineStep = units::psiFromKpa(config::kBaroBaselineStepKpa);
    }
    if (within(x, y, layout::kConfigMinusX, row0, layout::kConfigMinusX + layout::kConfigMinusW, row0End)) {
        configStore_.setBaroBaselinePsi(settings.baroBaselinePsi - baselineStep);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row0, layout::kConfigPlusX + layout::kConfigPlusW, row0End)) {
        configStore_.setBaroBaselinePsi(settings.baroBaselinePsi + baselineStep);
        return false;
    }

    int32_t row1 = layout::kConfigRow1Y + layout::kConfigButtonInsetY;
    int32_t row1End = row1 + layout::kConfigButtonH;
    float coolantStep = metric ? config::kCoolantWarningStepC : config::kCoolantWarningStepF;
    if (within(x, y, layout::kConfigMinusX, row1, layout::kConfigMinusX + layout::kConfigMinusW, row1End)) {
        configStore_.setCoolantWarningF(settings.coolantWarningF - coolantStep);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row1, layout::kConfigPlusX + layout::kConfigPlusW, row1End)) {
        configStore_.setCoolantWarningF(settings.coolantWarningF + coolantStep);
        return false;
    }

    int32_t row2 = layout::kConfigRow2Y + layout::kConfigButtonInsetY;
    int32_t row2End = row2 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row2, layout::kConfigMinusX + layout::kConfigMinusW, row2End)) {
        configStore_.setLowVoltageWarningV(settings.lowVoltageWarningV - config::kLowVoltageWarningStepV);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row2, layout::kConfigPlusX + layout::kConfigPlusW, row2End)) {
        configStore_.setLowVoltageWarningV(settings.lowVoltageWarningV + config::kLowVoltageWarningStepV);
        return false;
    }

    int32_t row3 = layout::kConfigRow3Y + layout::kConfigButtonInsetY;
    int32_t row3End = row3 + layout::kConfigButtonH;
    float targetStep = metric ? units::mphFromKph(config::kZeroSixtyTargetStepKph) : config::kZeroSixtyTargetStepMph;
    if (within(x, y, layout::kConfigMinusX, row3, layout::kConfigMinusX + layout::kConfigMinusW, row3End)) {
        configStore_.setZeroSixtyTargetMph(settings.zeroSixtyTargetMph - targetStep);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row3, layout::kConfigPlusX + layout::kConfigPlusW, row3End)) {
        configStore_.setZeroSixtyTargetMph(settings.zeroSixtyTargetMph + targetStep);
        return false;
    }

    int32_t row4 = layout::kConfigRow4Y + layout::kConfigButtonInsetY;
    int32_t row4End = row4 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row4, layout::kConfigMinusX + layout::kConfigMinusW, row4End)) {
        configStore_.setHpEstimationFactor(settings.hpEstimationFactor - config::kHpEstimationFactorStep);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row4, layout::kConfigPlusX + layout::kConfigPlusW, row4End)) {
        configStore_.setHpEstimationFactor(settings.hpEstimationFactor + config::kHpEstimationFactorStep);
        return false;
    }

    int32_t row5 = layout::kConfigRow5Y + layout::kConfigButtonInsetY;
    int32_t row5End = row5 + layout::kConfigButtonH;
    if (within(x, y, layout::kConfigMinusX, row5, layout::kConfigMinusX + layout::kConfigMinusW, row5End)) {
        configStore_.setFuelTrimRangePct(settings.fuelTrimRangePct - config::kFuelTrimRangeStepPct);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row5, layout::kConfigPlusX + layout::kConfigPlusW, row5End)) {
        configStore_.setFuelTrimRangePct(settings.fuelTrimRangePct + config::kFuelTrimRangeStepPct);
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
        step = static_cast<uint16_t>(std::lroundf(units::mphFromKph(config::kMaxSpeedStepKph)));
    }
    if (within(x, y, layout::kConfigMinusX, row3, layout::kConfigMinusX + layout::kConfigMinusW, row3End)) {
        configStore_.setMaxSpeedMph(settings.maxSpeedMph - step);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row3, layout::kConfigPlusX + layout::kConfigPlusW, row3End)) {
        configStore_.setMaxSpeedMph(settings.maxSpeedMph + step);
        return false;
    }

    int32_t row4 = layout::kConfigRow4Y + layout::kConfigButtonInsetY;
    int32_t row4End = row4 + layout::kConfigButtonH;
    float vacuumStep = metric ? units::inHgFromKpa(config::kVacuumMaxStepKpa) : config::kVacuumMaxStepInHg;
    if (within(x, y, layout::kConfigMinusX, row4, layout::kConfigMinusX + layout::kConfigMinusW, row4End)) {
        configStore_.setVacuumMaxInHg(settings.vacuumMaxInHg - vacuumStep);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row4, layout::kConfigPlusX + layout::kConfigPlusW, row4End)) {
        configStore_.setVacuumMaxInHg(settings.vacuumMaxInHg + vacuumStep);
        return false;
    }

    int32_t row5 = layout::kConfigRow5Y + layout::kConfigButtonInsetY;
    int32_t row5End = row5 + layout::kConfigButtonH;
    float boostStep = metric ? units::psiFromKpa(config::kBoostMaxStepKpa) : config::kBoostMaxStepPsi;
    if (within(x, y, layout::kConfigMinusX, row5, layout::kConfigMinusX + layout::kConfigMinusW, row5End)) {
        configStore_.setBoostMaxPsi(settings.boostMaxPsi - boostStep);
        return false;
    }
    if (within(x, y, layout::kConfigPlusX, row5, layout::kConfigPlusX + layout::kConfigPlusW, row5End)) {
        configStore_.setBoostMaxPsi(settings.boostMaxPsi + boostStep);
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
    if (within(x, y, layout::kConfigActionX, summaryRow, layout::kConfigActionX + layout::kConfigActionW,
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
        bool screenFlipChanged = configStore_.isScreenFlipDirty();

        configStore_.save();

        if (logUnitsChanged) {
            csvLogger_.setUnitsMetric(newMetricLogs);
            csvLogger_.deleteAllLogs();
            state.cfgLogSummaryNextScanMs = 0; // force the LOGS/MB summary to rescan and show the wipe immediately
        }

        if (screenFlipChanged) {
            // Delete the saved touch calibration so TouchManager::begin()
            // falls through to its existing auto-calibration routine on the
            // next boot, now running in the new (flipped) rotation.
            sdManager_.deleteTouchCalibration();
            strncpy(state.configStatusMessage, "SAVED - RESTARTING", sizeof(state.configStatusMessage) - 1);
            state.configStatusMessage[sizeof(state.configStatusMessage) - 1] = '\0';
            state.configStatusMessageSetAtMs = nowMs;
            // Force the message onto the screen now: the normal per-frame
            // redraw in loop() never gets a chance to run before the
            // restart below.
            clusterPages_.drawSavedFeedback(nowMs);
            delay(1200);
            ESP.restart();
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

    // A detail overlay is showing (see ClusterPages::drawPage5Dynamic): any
    // other tap in the content area closes it back to the list.
    if (state.dtcDetailIndex >= 0) {
        if (within(x, y, 20, layout::kDtcListY - 10, 460,
                    layout::kDtcListY - 10 + layout::kDtcListLineHeight * layout::kDtcListVisibleLines + 20)) {
            state.dtcDetailIndex = -1;
        }
        return false;
    }

    // Otherwise, a tap on a truncated row opens its full description.
    uint8_t contentLines = 1;
    if (state.dtcHaveResultDrawn == 1 && state.dtcListDrawn.count > 0) {
        contentLines = state.dtcListDrawn.count < layout::kDtcListVisibleLines ? state.dtcListDrawn.count
                                                                                : layout::kDtcListVisibleLines;
    }
    int32_t startY = layout::dtcContentStartY(contentLines);
    if (state.dtcHaveResultDrawn == 1 && state.dtcListDrawn.count > 0 && within(x, y, 20, startY, 460,
                startY + contentLines * layout::kDtcListLineHeight)) {
        uint8_t row = static_cast<uint8_t>((static_cast<int32_t>(y) - startY) / layout::kDtcListLineHeight);
        if (row < contentLines && row < layout::kDtcListVisibleLines && state.dtcLineTruncated[row]) {
            state.dtcDetailIndex = row;
        }
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
