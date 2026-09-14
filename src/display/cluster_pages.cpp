#include "display/cluster_pages.h"
#include "display/cluster_layout.h"
#include "labels.h"
#include "system/units.h"
#include <string.h>
#include <stdio.h>

ClusterPages::ClusterPages(TFT_eSPI& tft, ConfigStore& configStore, CsvLogger& csvLogger, ObdClient& obdClient)
    : tft_(tft), configStore_(configStore), csvLogger_(csvLogger), obdClient_(obdClient),
      theme_(getTheme(ThemeId::ModernFlat)) {}

// ---------------------------------------------------------------- Header --

void ClusterPages::drawHeader(ClusterPage page, bool milOn) {
    tft_.fillRect(0, 0, layout::kScreenWidth, layout::kHeaderHeight, theme_.panel);
    tft_.drawFastHLine(0, layout::kHeaderHeight, layout::kScreenWidth, theme_.bezel);

    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(2);
    tft_.drawString(labels::kNavPrev, (layout::kNavPrevX0 + layout::kNavPrevX1) / 2, layout::kHeaderHeight / 2);
    tft_.drawString(labels::kNavNext, (layout::kNavNextX0 + layout::kNavNextX1) / 2, layout::kHeaderHeight / 2);

    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kPageTitles[static_cast<uint8_t>(page)], layout::kNavTitleCenterX, layout::kHeaderHeight / 2);

    gaugewidgets::drawMilIndicator(tft_, layout::kMilCenterX, layout::kMilCenterY, milOn, theme_);
    gaugewidgets::drawModeToggleButton(tft_, layout::kModeToggleCenterX, layout::kModeToggleCenterY,
                                        !isConfigPage(page), theme_);
}

void ClusterPages::drawStatusStrip(ConnectionState connectionState) {
    const char* label = toString(connectionState);
    uint16_t color;
    switch (connectionState) {
        case ConnectionState::Live:
            color = theme_.liveActive;
            break;
        case ConnectionState::Reconnecting:
        case ConnectionState::ObdConnecting:
        case ConnectionState::DisplayReady:
            color = theme_.primaryGaugeArc;
            break;
        case ConnectionState::Boot:
        case ConnectionState::SdInit:
        case ConnectionState::Stale:
        case ConnectionState::Degraded:
            color = theme_.warningActive;
            break;
    }

    gaugewidgets::drawStatusBadge(tft_, layout::kBadgeX, layout::kBadgeY, layout::kBadgeW, layout::kBadgeH, label,
                                   color, theme_.background);
}

void ClusterPages::beginLargeText(uint8_t size) {
    if (theme_.useSevenSegmentFont) {
        tft_.setTextFont(7);
        tft_.setTextSize(1);
    } else {
        tft_.setTextSize(size);
    }
}

void ClusterPages::endLargeText() {
    tft_.setTextFont(1); // No-op when Font 7 was never selected.
}

// ---------------------------------------------------------------- Dispatch --

void ClusterPages::drawStatic(ClusterPage page, const TelemetrySnapshot& snapshot) {
    bool milOn = snapshot.milOn.valid && snapshot.milOn.value != 0.0F;
    drawHeader(page, milOn);
    runtimeState_.headerMilOnDrawn = static_cast<int8_t>(milOn);

    switch (page) {
        case ClusterPage::PrimaryCluster: drawPage1Static(); break;
        case ClusterPage::EngineLoadAirflow: drawPage2Static(); break;
        case ClusterPage::CarSpecificSensors: drawPage3Static(); break;
        case ClusterPage::PerformanceTelemetry: drawPage4Static(); break;
        case ClusterPage::ConfigUi: drawConfigUiStatic(); break;
        case ClusterPage::ConfigGauges: drawConfigGaugesStatic(); break;
        case ClusterPage::ConfigUserVars: drawConfigUserVarsStatic(); break;
        case ClusterPage::ConfigLogs: drawConfigLogsStatic(); break;
        case ClusterPage::ConfigObd: drawConfigObdStatic(); break;
        case ClusterPage::Diagnostics: drawPage6Static(); break;
        default: break;
    }
    if (isConfigPage(page)) {
        drawConfigFooterStatic();
    }
}

void ClusterPages::drawDynamic(ClusterPage page, const TelemetrySnapshot& snapshot,
                                ConnectionState connectionState, uint32_t nowMs) {
    bool milOn = snapshot.milOn.valid && snapshot.milOn.value != 0.0F;
    if (runtimeState_.headerMilOnDrawn != static_cast<int8_t>(milOn)) {
        gaugewidgets::drawMilIndicator(tft_, layout::kMilCenterX, layout::kMilCenterY, milOn, theme_);
        runtimeState_.headerMilOnDrawn = static_cast<int8_t>(milOn);
    }
    drawStatusStrip(connectionState);

    switch (page) {
        case ClusterPage::PrimaryCluster: drawPage1Dynamic(snapshot, nowMs); break;
        case ClusterPage::EngineLoadAirflow: drawPage2Dynamic(snapshot, nowMs); break;
        case ClusterPage::CarSpecificSensors: drawPage3Dynamic(snapshot, nowMs); break;
        case ClusterPage::PerformanceTelemetry: drawPage4Dynamic(snapshot, nowMs); break;
        case ClusterPage::ConfigUi: drawConfigUiDynamic(nowMs); break;
        case ClusterPage::ConfigGauges: drawConfigGaugesDynamic(nowMs); break;
        case ClusterPage::ConfigUserVars: drawConfigUserVarsDynamic(nowMs); break;
        case ClusterPage::ConfigLogs: drawConfigLogsDynamic(nowMs); break;
        case ClusterPage::ConfigObd: drawConfigObdDynamic(nowMs); break;
        case ClusterPage::Diagnostics: drawPage6Dynamic(snapshot, nowMs); break;
        default: break;
    }
    if (isConfigPage(page)) {
        drawConfigFooterDynamic(nowMs);
    }
}

void ClusterPages::updateBackgroundTelemetry(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    if (snapshot.speedMph.valid) {
        float speed = snapshot.speedMph.value;
        float threshold = configStore_.settings().zeroSixtyTargetMph;
        if (!runtimeState_.zeroToSixtyRunning && runtimeState_.lastSpeedMphForTimer <= 0.0F && speed > 0.0F) {
            runtimeState_.zeroToSixtyRunning = true;
            runtimeState_.zeroToSixtyStartMs = nowMs;
            runtimeState_.zeroToSixtyResultSec = -1.0F;
        }
        if (runtimeState_.zeroToSixtyRunning && speed >= threshold) {
            runtimeState_.zeroToSixtyRunning = false;
            runtimeState_.zeroToSixtyResultSec = (nowMs - runtimeState_.zeroToSixtyStartMs) / 1000.0F;
        }
        runtimeState_.lastSpeedMphForTimer = speed;
    }

    constexpr uint32_t kMafSampleIntervalMs = 1000;
    if (nowMs - runtimeState_.lastMafSampleMs >= kMafSampleIntervalMs) {
        runtimeState_.lastMafSampleMs = nowMs;
        float sample = snapshot.mafGps.valid ? snapshot.mafGps.value : 0.0F;
        if (runtimeState_.mafHistoryCount < ClusterPageRuntimeState::kMafHistorySize) {
            runtimeState_.mafHistory[runtimeState_.mafHistoryCount++] = sample;
        } else {
            memmove(runtimeState_.mafHistory, runtimeState_.mafHistory + 1,
                    (ClusterPageRuntimeState::kMafHistorySize - 1) * sizeof(float));
            runtimeState_.mafHistory[ClusterPageRuntimeState::kMafHistorySize - 1] = sample;
        }
    }
}

// ---------------------------------------------------------------- Page 1: Primary Cluster --

void ClusterPages::drawPage1Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    rpmArc_.invalidate();
    speedArc_.invalidate();
    throttleBar_.invalidate();

    // Must match the RPM/Speed gauge geometry in drawPage1Dynamic().
    constexpr int32_t kRpmGaugeCx = 130, kSpeedGaugeCx = 350, kGaugeCy = 145, kGaugeRadius = 95;
    gaugewidgets::drawGaugeBezel(tft_, kRpmGaugeCx, kGaugeCy, kGaugeRadius, theme_);
    gaugewidgets::drawGaugeBezel(tft_, kSpeedGaugeCx, kGaugeCy, kGaugeRadius, theme_);

    constexpr int32_t kRowY = 250, kRowH = 60, kColW = 154, kColGap = 5;
    for (int i = 0; i < 3; ++i) {
        int32_t x = 4 + i * (kColW + kColGap);
        tft_.fillRoundRect(x, kRowY, kColW, kRowH, 6, theme_.panel);
    }

    // Fixed unit captions for Coolant/IAT, drawn once here so they never shift
    // or get redrawn when the temperature's digit count changes; centered
    // with a reserved value width so the [value][unit] pair reads as
    // centered in its box instead of hugging the right edge. Must match the
    // layout in drawPage1Dynamic().
    // "999" rather than a more "realistic" value like "199": Torque Neon's
    // Orbitron value font is proportional, and digit '1' renders half as wide
    // as the others, so a rep value containing '1' underestimates the true
    // widest-digit-combo width and leaves stale fragments on repaint.
    constexpr const char* kTempRepValue = "999"; // widest expected coolant/IAT reading
    bool metric = configStore_.settings().useMetricUnits;
    int32_t coolantColX = 4 + 0 * (kColW + kColGap);
    int32_t coolantUnitW = gaugewidgets::fixedUnitWidth(tft_, units::tempUnitLabel(metric), 2);
    int32_t coolantValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 2, kTempRepValue);
    gaugewidgets::ValueUnitGroup coolantGroup =
        gaugewidgets::centerValueUnitGroup(coolantColX + kColW / 2, coolantValueW, coolantUnitW, 4);
    gaugewidgets::drawFixedUnit(tft_, units::tempUnitLabel(metric), coolantGroup.unitRightX, kRowY + 26, TR_DATUM, 2,
                                 theme_, theme_.panel);

    int32_t iatColX = 4 + 2 * (kColW + kColGap);
    gaugewidgets::drawValueBoxStatic(tft_, iatColX, kRowY + 8, kColW, labels::kLabelIat, units::tempUnitLabel(metric),
                                      kTempRepValue, theme_, 1, 2, 18);
}

void ClusterPages::drawPage1Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    constexpr int32_t kRpmGaugeCx = 130, kSpeedGaugeCx = 350, kGaugeCy = 145, kGaugeRadius = 95;

    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    const float kRpmMax = static_cast<float>(settings.maxRpm);
    const float kSpeedMax = units::displaySpeed(static_cast<float>(settings.maxSpeedMph), metric);

    float targetRpm = snapshot.rpm.valid ? snapshot.rpm.value : 0.0F;
    float targetSpeed = snapshot.speedMph.valid ? units::displaySpeed(snapshot.speedMph.value, metric) : 0.0F;
    float dt = (lastFrameMs_ == 0) ? 0.033F : (nowMs - lastFrameMs_) / 1000.0F;
    if (dt > 0.25F) {
        dt = 0.25F; // Clamp huge gaps (page switches, stalls) so the spring doesn't overshoot.
    }
    rpmNeedle_.update(targetRpm, dt);
    speedNeedle_.update(targetSpeed, dt);
    lastFrameMs_ = nowMs;

    // Orange from Shift Light RPM (Page 5) up to Redline RPM, then solid red
    // from Redline RPM out to the end of the sweep.
    gaugewidgets::drawArcGauge(tft_, rpmArc_, kRpmGaugeCx, kGaugeCy, kGaugeRadius, rpmNeedle_.currentValue, kRpmMax,
                                static_cast<float>(settings.shiftLightRpm),
                                static_cast<float>(settings.redlineRpm), theme_.background, theme_);
    gaugewidgets::drawArcGauge(tft_, speedArc_, kSpeedGaugeCx, kGaugeCy, kGaugeRadius, speedNeedle_.currentValue,
                                kSpeedMax, kSpeedMax, kSpeedMax, theme_.background, theme_);

    TickMode tickMode = static_cast<TickMode>(settings.tickMode);
    if (tickMode != TickMode::Off) {
        gaugewidgets::drawGaugeTicks(tft_, kRpmGaugeCx, kGaugeCy, kGaugeRadius, rpmNeedle_.currentValue, kRpmMax,
                                      config::kRpmTickIntervalMinor, config::kRpmTickIntervalMajor,
                                      static_cast<float>(settings.redlineRpm), tickMode, theme_);
        gaugewidgets::drawGaugeTicks(tft_, kSpeedGaugeCx, kGaugeCy, kGaugeRadius, speedNeedle_.currentValue,
                                      kSpeedMax, config::kSpeedTickIntervalMinor, config::kSpeedTickIntervalMajor,
                                      kSpeedMax, tickMode, theme_);
    }

    bool shiftLightOn = snapshot.rpm.valid && snapshot.rpm.value >= settings.shiftLightRpm;
    bool flashPhase = ((nowMs / 200) % 2) == 0;
    uint16_t rpmTextColor = (shiftLightOn && flashPhase) ? theme_.warningActive : theme_.textPrimary;

    char rpmBuf[8];
    if (snapshot.rpm.valid) {
        snprintf(rpmBuf, sizeof(rpmBuf), "%d", static_cast<int>(rpmNeedle_.currentValue));
    } else {
        strcpy(rpmBuf, "--");
    }
    // Reserve the field width from the widest expected reading (RPM can reach
    // 4 digits, e.g. up to kMaxMaxRpm = 9000) rather than a guessed constant:
    // Mustang S197's 7-segment font is wider per digit than other themes'
    // fonts, so a fixed 120px field left part of a 4-digit value undrawn by
    // the clear rect, leaving stale segments once the value dropped back to
    // 3 digits.
    constexpr const char* kRpmRepValue = "9000";
    int32_t rpmFieldWidth = gaugewidgets::reservedValueWidth(tft_, theme_, 4, kRpmRepValue);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(rpmTextColor, theme_.background);
    applyValueFont(tft_, theme_, 4);
    gaugewidgets::drawFieldText(tft_, rpmBuf, kRpmGaugeCx, kGaugeCy - 5, rpmFieldWidth, theme_.background);
    resetValueFont(tft_);
    tft_.setTextSize(2);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    int32_t rpmLabelY = kGaugeCy + 35 + (theme_.numberedFonts[2] != 0 ? 8 : 0);
    tft_.drawString(labels::kUnitRpm, kRpmGaugeCx, rpmLabelY);

    char speedBuf[8];
    if (snapshot.speedMph.valid) {
        snprintf(speedBuf, sizeof(speedBuf), "%d", static_cast<int>(speedNeedle_.currentValue));
    } else {
        strcpy(speedBuf, "--");
    }
    // See kRpmRepValue above: reserve the field from the widest expected
    // reading rather than a guessed constant. Speed tops out at 3 digits even
    // in km/h at the max configurable speed.
    constexpr const char* kSpeedRepValue = "999";
    int32_t speedFieldWidth = gaugewidgets::reservedValueWidth(tft_, theme_, 4, kSpeedRepValue);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyValueFont(tft_, theme_, 4);
    gaugewidgets::drawFieldText(tft_, speedBuf, kSpeedGaugeCx, kGaugeCy - 5, speedFieldWidth, theme_.background);
    resetValueFont(tft_);
    tft_.setTextSize(2);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    int32_t speedLabelY = kGaugeCy + 35 + (theme_.numberedFonts[2] != 0 ? 8 : 0);
    tft_.drawString(units::speedUnitLabel(metric), kSpeedGaugeCx, speedLabelY);

    constexpr int32_t kRowY = 250, kColW = 154, kColGap = 5;
    char valueBuf[16];

    constexpr const char* kTempRepValue = "999"; // must match drawPage1Static()
    bool coolantHot = snapshot.coolantF.valid && snapshot.coolantF.value > settings.coolantWarningF;
    float displayCoolant = units::displayTemp(snapshot.coolantF.value, metric);
    snprintf(valueBuf, sizeof(valueBuf), "%d", static_cast<int>(displayCoolant));
    int32_t coolantColX = 4 + 0 * (kColW + kColGap);
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelCoolant, coolantColX + kColW / 2, kRowY + 8);
    int32_t coolantUnitW = gaugewidgets::fixedUnitWidth(tft_, units::tempUnitLabel(metric), 2);
    int32_t coolantValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 2, kTempRepValue);
    gaugewidgets::ValueUnitGroup coolantGroup =
        gaugewidgets::centerValueUnitGroup(coolantColX + kColW / 2, coolantValueW, coolantUnitW, 4);
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextColor(coolantHot ? theme_.warningActive
                                  : (snapshot.coolantF.valid ? theme_.textPrimary : theme_.textSecondary),
                       theme_.panel);
    applyValueFont(tft_, theme_, 2);
    gaugewidgets::drawFieldText(tft_, snapshot.coolantF.valid ? valueBuf : "--", coolantGroup.valueRightX, kRowY + 26,
                                 coolantValueW, theme_.panel);
    resetValueFont(tft_);

    float displayIat = units::displayTemp(snapshot.iatF.value, metric);
    snprintf(valueBuf, sizeof(valueBuf), "%d", static_cast<int>(displayIat));
    gaugewidgets::drawValueBoxValue(tft_, 4 + 2 * (kColW + kColGap), kRowY + 8, kColW, valueBuf,
                                     units::tempUnitLabel(metric), kTempRepValue, snapshot.iatF.valid, theme_, 2, 18);

    int32_t throttleColX = 4 + 1 * (kColW + kColGap);
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelThrottle, throttleColX + kColW / 2, kRowY + 8);
    gaugewidgets::drawBarGauge(tft_, throttleBar_, throttleColX + 8, kRowY + 24, kColW - 16, 24,
                                snapshot.throttlePct.valid ? snapshot.throttlePct.value : 0.0F,
                                theme_.primaryGaugeArc, theme_);
}

// ---------------------------------------------------------------- Page 2: Engine Load & Airflow --

ClusterPages::MafTimingLayout ClusterPages::computeMafTimingLayout() {
    constexpr int32_t kBoxY = 50;
    constexpr int32_t kTopPad = 8;               // box top -> MAF label
    constexpr int32_t kMafLabelToValueGap = 22;  // MAF label->value gap (wider than drawValueBox()'s 14px default)
    constexpr int32_t kTaLabelToValueGap = 22;   // Timing Advance label->value gap (wider than the 14px default)
    constexpr int32_t kPeakTextHeight = 8;       // size-1 default-font line height
    constexpr int32_t kLineGap = 8;             // MAF value -> PEAK text (same group)
    constexpr int32_t kGroupGap = 16;            // PEAK text -> TIMING ADVANCE label (new group)
    constexpr int32_t kBottomPad = 8;            // TIMING ADVANCE value -> box bottom

    applyValueFont(tft_, theme_, 2);
    int32_t valueFontHeight = tft_.fontHeight();
    resetValueFont(tft_);

    MafTimingLayout layoutOut;
    layoutOut.mafLabelY = kBoxY + kTopPad;
    layoutOut.mafLabelToValueGap = kMafLabelToValueGap;
    int32_t mafValueBottom = layoutOut.mafLabelY + kMafLabelToValueGap + valueFontHeight;
    layoutOut.peakY = mafValueBottom + kLineGap;
    int32_t peakBottom = layoutOut.peakY + kPeakTextHeight;
    layoutOut.taLabelY = peakBottom + kGroupGap;
    layoutOut.taLabelToValueGap = kTaLabelToValueGap;
    int32_t taValueBottom = layoutOut.taLabelY + kTaLabelToValueGap + valueFontHeight;
    layoutOut.boxHeight = (taValueBottom + kBottomPad) - kBoxY;
    return layoutOut;
}

void ClusterPages::drawPage2Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    loadArc_.invalidate();
    stftBar_.invalidate();
    ltftBar_.invalidate();

    // Must match the Engine Load gauge geometry in drawPage2Dynamic().
    constexpr int32_t kGaugeCx = 130, kGaugeCy = 130, kGaugeRadius = 78;
    gaugewidgets::drawGaugeBezel(tft_, kGaugeCx, kGaugeCy, kGaugeRadius, theme_);

    // Fixed "%" unit for Engine Load, drawn once so it never shifts or gets
    // redrawn when the load's digit count changes; centered with a reserved
    // value width so the pair reads as centered on the gauge instead of
    // hugging its edge. Must match the layout in drawPage2Dynamic().
    int32_t loadUnitW = gaugewidgets::fixedUnitWidth(tft_, "%", 2);
    int32_t loadValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 5, "100");
    gaugewidgets::ValueUnitGroup loadGroup = gaugewidgets::centerValueUnitGroup(kGaugeCx, loadValueW, loadUnitW, 4);
    gaugewidgets::drawFixedUnit(tft_, "%", loadGroup.unitRightX, kGaugeCy, MR_DATUM, 2, theme_, theme_.background);

    // Must match the MAF/TIMING ADVANCE layout in drawPage2Dynamic().
    MafTimingLayout mafLayout = computeMafTimingLayout();
    tft_.fillRoundRect(250, 50, 220, mafLayout.boxHeight, 6, theme_.panel); // MAF + TIMING ADVANCE box
    tft_.fillRoundRect(20, 220, 210, 80, 6, theme_.panel);
    tft_.fillRoundRect(250, 220, 210, 80, 6, theme_.panel);

    // Fixed labels/units for MAF, PEAK, and TIMING ADVANCE, drawn once so
    // they never shift when the numbers between them change width; centered
    // the same way. Must match the layout in drawPage2Dynamic().
    gaugewidgets::drawValueBoxStatic(tft_, 260, mafLayout.mafLabelY, 200, labels::kLabelMaf, "g/s", "99.9", theme_, 2,
                                      2, mafLayout.mafLabelToValueGap);

    // "PEAK ## g/s": prefix/value-slot/suffix centered on the box's original
    // center (360) instead of anchored to the box edges.
    constexpr int32_t kPeakCenterX = 360;
    constexpr int32_t kPeakGapPx = 4;
    int32_t peakPrefixW = gaugewidgets::fixedUnitWidth(tft_, "PEAK", 1);
    int32_t peakSuffixW = gaugewidgets::fixedUnitWidth(tft_, "g/s", 1);
    int32_t peakValueW = gaugewidgets::fixedUnitWidth(tft_, "99.9", 1);
    int32_t peakGroupWidth = peakPrefixW + kPeakGapPx + peakValueW + kPeakGapPx + peakSuffixW;
    int32_t peakGroupLeft = kPeakCenterX - peakGroupWidth / 2;
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.setTextFont(1);
    tft_.drawString("PEAK", peakGroupLeft, mafLayout.peakY);
    gaugewidgets::drawFixedUnit(tft_, "g/s", peakGroupLeft + peakGroupWidth, mafLayout.peakY, TR_DATUM, 1, theme_,
                                 theme_.panel);

    gaugewidgets::drawValueBoxStatic(tft_, 260, mafLayout.taLabelY, 200, labels::kLabelTimingAdvance, "deg", "-45.0",
                                      theme_, 2, 2, mafLayout.taLabelToValueGap);

    // Fixed labels/units for STFT/LTFT, same reasoning; must match the layout
    // in drawPage2Dynamic().
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.setTextFont(1);
    tft_.drawString(labels::kLabelStftBank1, 125, 228);
    tft_.drawString(labels::kLabelLtftBank1, 355, 228);
    int32_t trimUnitW = gaugewidgets::fixedUnitWidth(tft_, "%", 1);
    int32_t trimValueW = gaugewidgets::fixedUnitWidth(tft_, "-25.0", 1);
    gaugewidgets::ValueUnitGroup stftGroup = gaugewidgets::centerValueUnitGroup(125, trimValueW, trimUnitW, 4);
    gaugewidgets::ValueUnitGroup ltftGroup = gaugewidgets::centerValueUnitGroup(355, trimValueW, trimUnitW, 4);
    gaugewidgets::drawFixedUnit(tft_, "%", stftGroup.unitRightX, 288, TR_DATUM, 1, theme_, theme_.panel);
    gaugewidgets::drawFixedUnit(tft_, "%", ltftGroup.unitRightX, 288, TR_DATUM, 1, theme_, theme_.panel);
}

void ClusterPages::drawPage2Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    constexpr int32_t kGaugeCx = 130, kGaugeCy = 130, kGaugeRadius = 78;
    char buf[24];

    float targetLoad = snapshot.engineLoadPct.valid ? snapshot.engineLoadPct.value : 0.0F;
    gaugewidgets::drawArcGauge(tft_, loadArc_, kGaugeCx, kGaugeCy, kGaugeRadius, targetLoad, 100.0F, 100.0F, 100.0F,
                                theme_.background, theme_);

    snprintf(buf, sizeof(buf), "%d", static_cast<int>(targetLoad));
    int32_t loadUnitW = gaugewidgets::fixedUnitWidth(tft_, "%", 2);
    int32_t loadValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 5, "100");
    gaugewidgets::ValueUnitGroup loadGroup = gaugewidgets::centerValueUnitGroup(kGaugeCx, loadValueW, loadUnitW, 4);
    tft_.setTextDatum(MR_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyValueFont(tft_, theme_, 5);
    gaugewidgets::drawFieldText(tft_, snapshot.engineLoadPct.valid ? buf : "--", loadGroup.valueRightX, kGaugeCy,
                                 loadValueW, theme_.background);
    resetValueFont(tft_);
    tft_.setTextSize(1);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(labels::kLabelEngineLoad, kGaugeCx, kGaugeCy + 34);

    // Must match the box geometry in drawPage2Static().
    MafTimingLayout mafLayout = computeMafTimingLayout();

    if (snapshot.mafGps.valid && snapshot.mafGps.value > runtimeState_.mafPeakGps) {
        runtimeState_.mafPeakGps = snapshot.mafGps.value;
    }
    snprintf(buf, sizeof(buf), "%.1f", snapshot.mafGps.value);
    gaugewidgets::drawValueBoxValue(tft_, 260, mafLayout.mafLabelY, 200, buf, "g/s", "99.9", snapshot.mafGps.valid,
                                     theme_, 2, mafLayout.mafLabelToValueGap);

    // Must match the "PEAK ## g/s" layout in drawPage2Static().
    char peakBuf[24];
    constexpr int32_t kPeakCenterX = 360;
    constexpr int32_t kPeakGapPx = 4;
    int32_t peakPrefixW = gaugewidgets::fixedUnitWidth(tft_, "PEAK", 1);
    int32_t peakSuffixW = gaugewidgets::fixedUnitWidth(tft_, "g/s", 1);
    int32_t peakValueW = gaugewidgets::fixedUnitWidth(tft_, "99.9", 1);
    int32_t peakGroupWidth = peakPrefixW + kPeakGapPx + peakValueW + kPeakGapPx + peakSuffixW;
    int32_t peakGroupLeft = kPeakCenterX - peakGroupWidth / 2;
    snprintf(peakBuf, sizeof(peakBuf), "%.1f", runtimeState_.mafPeakGps);
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    gaugewidgets::drawFieldText(tft_, peakBuf, peakGroupLeft + peakPrefixW + kPeakGapPx, mafLayout.peakY, peakValueW,
                                 theme_.panel);

    snprintf(buf, sizeof(buf), "%.1f", snapshot.timingAdvanceDeg.value);
    gaugewidgets::drawValueBoxValue(tft_, 260, mafLayout.taLabelY, 200, buf, "deg", "-45.0",
                                     snapshot.timingAdvanceDeg.valid, theme_, 2, mafLayout.taLabelToValueGap);

    int32_t trimUnitW = gaugewidgets::fixedUnitWidth(tft_, "%", 1);
    int32_t trimValueW = gaugewidgets::fixedUnitWidth(tft_, "-25.0", 1);

    // Map -range..+range onto the 0-100% bar widget so the fill visually centers.
    float stftCentered =
        snapshot.stftPct.valid ? (snapshot.stftPct.value + settings.fuelTrimRangePct) * (50.0F / settings.fuelTrimRangePct) : 0.0F;
    gaugewidgets::drawBarGauge(tft_, stftBar_, 30, 250, 190, 30, stftCentered, theme_.primaryGaugeArc, theme_);
    gaugewidgets::ValueUnitGroup stftGroup = gaugewidgets::centerValueUnitGroup(125, trimValueW, trimUnitW, 4);
    snprintf(buf, sizeof(buf), "%.1f", snapshot.stftPct.value);
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.panel);
    tft_.setTextSize(1);
    gaugewidgets::drawFieldText(tft_, snapshot.stftPct.valid ? buf : "--", stftGroup.valueRightX, 288, trimValueW,
                                 theme_.panel);

    float ltftCentered =
        snapshot.ltftPct.valid ? (snapshot.ltftPct.value + settings.fuelTrimRangePct) * (50.0F / settings.fuelTrimRangePct) : 0.0F;
    gaugewidgets::drawBarGauge(tft_, ltftBar_, 260, 250, 190, 30, ltftCentered, theme_.primaryGaugeArc, theme_);
    gaugewidgets::ValueUnitGroup ltftGroup = gaugewidgets::centerValueUnitGroup(355, trimValueW, trimUnitW, 4);
    snprintf(buf, sizeof(buf), "%.1f", snapshot.ltftPct.value);
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.panel);
    tft_.setTextSize(1);
    gaugewidgets::drawFieldText(tft_, snapshot.ltftPct.valid ? buf : "--", ltftGroup.valueRightX, 288, trimValueW,
                                 theme_.panel);
}

// ---------------------------------------------------------------- Page 3: Car-Specific Sensors --

ClusterPages::VacuumLayout ClusterPages::computeVacuumLayout() {
    constexpr int32_t kBoxTop = 130; // 10px below the Battery/Fuel Rail Pressure boxes (y=50, height 70)
    constexpr int32_t kTopPad = 8;
    constexpr int32_t kLabelHeight = 8; // size-1 default-font line height
    constexpr int32_t kBarHeight = 30;
    constexpr int32_t kLabelToBarGap = 4;
    constexpr int32_t kBarToValueGap = 6;
    constexpr int32_t kBottomPad = 8;

    applyValueFont(tft_, theme_, 3);
    int32_t valueHeight = tft_.fontHeight();
    resetValueFont(tft_);

    VacuumLayout layoutOut;
    layoutOut.labelY = kBoxTop + kTopPad;
    layoutOut.barY = layoutOut.labelY + kLabelHeight + kLabelToBarGap;
    layoutOut.valueY = layoutOut.barY + kBarHeight + kBarToValueGap;
    layoutOut.boxHeight = (layoutOut.valueY + valueHeight + kBottomPad) - kBoxTop;
    return layoutOut;
}

void ClusterPages::drawPage3Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    vacuumBar_.invalidate();

    tft_.fillRoundRect(10, 50, 225, 70, 6, theme_.panel);
    tft_.fillRoundRect(245, 50, 225, 70, 6, theme_.panel);
    tft_.fillRoundRect(10, 245, 140, 65, 6, theme_.panel);
    tft_.fillRoundRect(330, 245, 140, 65, 6, theme_.panel);

    // Fixed units, drawn once so they never shift or get redrawn when their
    // paired number's digit count changes; centered with a reserved value
    // width so each pair reads as centered in its box. Must match the layout
    // in drawPage3Dynamic().
    bool metric = configStore_.settings().useMetricUnits;
    int32_t voltageUnitW = gaugewidgets::fixedUnitWidth(tft_, "V", 2);
    int32_t voltageValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 2, "19.99");
    gaugewidgets::ValueUnitGroup voltageGroup =
        gaugewidgets::centerValueUnitGroup(122, voltageValueW, voltageUnitW, 4);
    gaugewidgets::drawFixedUnit(tft_, "V", voltageGroup.unitRightX, 80, TR_DATUM, 2, theme_, theme_.panel);
    gaugewidgets::drawValueBoxStatic(tft_, 245, 58, 225, labels::kLabelFuelRailPressure,
                                      units::pressureUnitLabel(metric), "999", theme_, 1, 2, 22);
    gaugewidgets::drawValueBoxStatic(tft_, 10, 253, 140, labels::kLabelO2B1S1, "V", "8.88", theme_, 1, 2, 22);
    gaugewidgets::drawValueBoxStatic(tft_, 330, 253, 140, labels::kLabelO2B2S1, "V", "8.88", theme_, 1, 2, 22);

    // Vacuum/Boost panel - fills the gap between the four corner panels above.
    // Must match the layout in drawPage3Dynamic().
    VacuumLayout vacLayout = computeVacuumLayout();
    tft_.fillRoundRect(150, 130, 180, vacLayout.boxHeight, 6, theme_.panel);
}

void ClusterPages::drawPage3Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    char buf[20];

    bool lowVoltage = snapshot.voltageV.valid && snapshot.voltageV.value < settings.lowVoltageWarningV;
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelBatteryVoltage, 122, 58);
    int32_t voltageUnitW = gaugewidgets::fixedUnitWidth(tft_, "V", 2);
    int32_t voltageValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 2, "19.99");
    gaugewidgets::ValueUnitGroup voltageGroup =
        gaugewidgets::centerValueUnitGroup(122, voltageValueW, voltageUnitW, 4);
    snprintf(buf, sizeof(buf), "%.2f", snapshot.voltageV.value);
    tft_.setTextDatum(TR_DATUM);
    tft_.setTextColor(lowVoltage ? theme_.warningActive
                                  : (snapshot.voltageV.valid ? theme_.textPrimary : theme_.textSecondary),
                       theme_.panel);
    applyValueFont(tft_, theme_, 2);
    gaugewidgets::drawFieldText(tft_, snapshot.voltageV.valid ? buf : "--", voltageGroup.valueRightX, 80,
                                 voltageValueW, theme_.panel);
    resetValueFont(tft_);

    float displayFuelPressure = units::displayPressureFromKpa(snapshot.fuelPressureKpa.value, metric);
    snprintf(buf, sizeof(buf), "%d", static_cast<int>(displayFuelPressure));
    gaugewidgets::drawValueBoxValue(tft_, 245, 58, 225, buf, units::pressureUnitLabel(metric), "999",
                                     snapshot.fuelPressureKpa.valid, theme_, 2, 22);

    // Vacuum (MAP < baro baseline) or boost (MAP > baseline), per the Page 3 spec.
    float baroBaselineKpa = units::kpaFromPsi(settings.baroBaselinePsi);
    bool haveMap = snapshot.mapKpa.valid;
    float mapKpa = snapshot.mapKpa.value;
    bool isBoost = haveMap && mapKpa > baroBaselineKpa;
    float gaugeValue = 0.0F, gaugeMax = 1.0F;
    const char* gaugeLabel = labels::kLabelVacBoost;
    const char* gaugeUnit = "";
    if (haveMap) {
        if (isBoost) {
            if (metric) {
                gaugeValue = mapKpa - baroBaselineKpa;
                gaugeMax = units::kpaFromPsi(settings.boostMaxPsi);
            } else {
                gaugeValue = units::psiFromKpa(mapKpa - baroBaselineKpa);
                gaugeMax = settings.boostMaxPsi;
            }
            gaugeLabel = labels::kLabelBoost;
            gaugeUnit = metric ? labels::kUnitKpa : labels::kUnitPsi;
        } else {
            if (metric) {
                gaugeValue = baroBaselineKpa - mapKpa;
                gaugeMax = units::kpaFromInHg(settings.vacuumMaxInHg);
            } else {
                gaugeValue = units::inHgFromKpa(baroBaselineKpa - mapKpa);
                gaugeMax = settings.vacuumMaxInHg;
            }
            gaugeLabel = labels::kLabelVacuum;
            gaugeUnit = metric ? labels::kUnitKpa : labels::kUnitInHg;
        }
    }
    // Fills the panel box between the four corner panels (x 150..330).
    constexpr int32_t kVacCx = 240;
    constexpr int32_t kVacBarWidth = 170, kVacBarHeight = 30;
    constexpr int32_t kVacFieldWidth = 160;

    // Must match the box geometry in drawPage3Static().
    VacuumLayout vacLayout = computeVacuumLayout();

    tft_.setTextDatum(TC_DATUM);
    tft_.setTextSize(1);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    // Font 7 (used for the value under some themes) can't render letters, so
    // the unit rides on the label line instead of the value line.
    char labelBuf[24];
    if (haveMap) {
        snprintf(labelBuf, sizeof(labelBuf), "%s (%s)", gaugeLabel, gaugeUnit);
    } else {
        snprintf(labelBuf, sizeof(labelBuf), "%s", gaugeLabel);
    }
    gaugewidgets::drawFieldText(tft_, labelBuf, kVacCx, vacLayout.labelY, 170, theme_.panel);

    float vacPercent = haveMap ? (gaugeValue / gaugeMax) * 100.0F : 0.0F;
    gaugewidgets::drawBarGauge(tft_, vacuumBar_, kVacCx - kVacBarWidth / 2, vacLayout.barY, kVacBarWidth,
                                kVacBarHeight, vacPercent, theme_.primaryGaugeArc, theme_);

    snprintf(buf, sizeof(buf), "%.1f", gaugeValue);
    tft_.setTextColor(theme_.textPrimary, theme_.panel);
    applyValueFont(tft_, theme_, 3);
    gaugewidgets::drawFieldText(tft_, haveMap ? buf : "--", kVacCx, vacLayout.valueY, kVacFieldWidth, theme_.panel);
    resetValueFont(tft_);
    tft_.setTextSize(1);

    snprintf(buf, sizeof(buf), "%.2f", snapshot.o2B1S1V.value);
    gaugewidgets::drawValueBoxValue(tft_, 10, 253, 140, buf, "V", "8.88", snapshot.o2B1S1V.valid, theme_, 2, 22);

    snprintf(buf, sizeof(buf), "%.2f", snapshot.o2B2S1V.value);
    gaugewidgets::drawValueBoxValue(tft_, 330, 253, 140, buf, "V", "8.88", snapshot.o2B2S1V.valid, theme_, 2, 22);
}

// ---------------------------------------------------------------- Page 4: Performance & Telemetry --

void ClusterPages::drawPage4Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    tft_.fillRoundRect(10, 50, 220, 65, 6, theme_.panel);
    tft_.fillRoundRect(250, 50, 220, 65, 6, theme_.panel);
    tft_.fillRoundRect(140, 125, 200, 70, 6, theme_.panel);
    tft_.drawRoundRect(10, 205, 460, 110, 6, theme_.bezel);
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelIntakeAirflow60s, 16, 211);

    // Fixed labels/units, drawn once so they never shift or get redrawn when
    // their paired number's digit count changes; centered with a reserved
    // value width so each pair reads as centered in its box. Must match the
    // layout in drawPage4Dynamic().
    gaugewidgets::drawValueBoxStatic(tft_, 10, 58, 220, labels::kLabelEstHorsepower, "HP", "999", theme_, 1, 2, 22);
    gaugewidgets::drawValueBoxStatic(tft_, 250, 58, 220, labels::kLabelEstTorque, "lb-ft", "999", theme_, 1, 2, 22);
    int32_t timerUnitW = gaugewidgets::fixedUnitWidth(tft_, "s", 2);
    int32_t timerValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 2, "99.99");
    gaugewidgets::ValueUnitGroup timerGroup = gaugewidgets::centerValueUnitGroup(240, timerValueW, timerUnitW, 4);
    gaugewidgets::drawFixedUnit(tft_, "s", timerGroup.unitRightX, 155, TR_DATUM, 2, theme_, theme_.panel);
}

void ClusterPages::drawPage4Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    char buf[20];

    bool haveMaf = snapshot.mafGps.valid;
    bool haveRpm = snapshot.rpm.valid && snapshot.rpm.value >= 500.0F;
    float hp = haveMaf ? snapshot.mafGps.value * settings.hpEstimationFactor : 0.0F;
    float torque = (haveMaf && haveRpm) ? (hp * 5252.0F / snapshot.rpm.value) : 0.0F;

    snprintf(buf, sizeof(buf), "%.0f", hp);
    gaugewidgets::drawValueBoxValue(tft_, 10, 58, 220, buf, "HP", "999", haveMaf, theme_, 2, 22);

    snprintf(buf, sizeof(buf), "%.0f", torque);
    gaugewidgets::drawValueBoxValue(tft_, 250, 58, 220, buf, "lb-ft", "999", haveMaf && haveRpm, theme_, 2, 22);

    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    const char* timerLabel = metric ? labels::kLabelZeroToHundredKph : labels::kLabelZeroToSixty;
    tft_.drawString(timerLabel, 240, 132);
    int32_t timerUnitW = gaugewidgets::fixedUnitWidth(tft_, "s", 2);
    int32_t timerValueW = gaugewidgets::reservedValueWidth(tft_, theme_, 2, "99.99");
    gaugewidgets::ValueUnitGroup timerGroup = gaugewidgets::centerValueUnitGroup(240, timerValueW, timerUnitW, 4);
    if (runtimeState_.zeroToSixtyRunning) {
        float elapsed = (nowMs - runtimeState_.zeroToSixtyStartMs) / 1000.0F;
        snprintf(buf, sizeof(buf), "%.2f", elapsed);
        tft_.setTextColor(theme_.primaryGaugeArc, theme_.panel);
    } else if (runtimeState_.zeroToSixtyResultSec >= 0.0F) {
        snprintf(buf, sizeof(buf), "%.2f", runtimeState_.zeroToSixtyResultSec);
        tft_.setTextColor(theme_.textPrimary, theme_.panel);
    } else {
        strcpy(buf, "--");
        tft_.setTextColor(theme_.textSecondary, theme_.panel);
    }
    tft_.setTextDatum(TR_DATUM);
    applyValueFont(tft_, theme_, 2);
    gaugewidgets::drawFieldText(tft_, buf, timerGroup.valueRightX, 155, timerValueW, theme_.panel);
    resetValueFont(tft_);

    constexpr int32_t kGraphX = 12, kGraphY = 227, kGraphW = 456, kGraphH = 84;
    tft_.fillRect(kGraphX, kGraphY, kGraphW, kGraphH, theme_.background);
    uint8_t count = runtimeState_.mafHistoryCount;
    if (count >= 2) {
        float maxSeen = 1.0F;
        for (uint8_t i = 0; i < count; ++i) {
            if (runtimeState_.mafHistory[i] > maxSeen) {
                maxSeen = runtimeState_.mafHistory[i];
            }
        }
        for (uint8_t i = 0; i < count - 1; ++i) {
            int32_t xA = kGraphX + ((kGraphW - 1) * i) / (ClusterPageRuntimeState::kMafHistorySize - 1);
            int32_t xB = kGraphX + ((kGraphW - 1) * (i + 1)) / (ClusterPageRuntimeState::kMafHistorySize - 1);
            int32_t yA = kGraphY + (kGraphH - 1) -
                         static_cast<int32_t>((runtimeState_.mafHistory[i] / maxSeen) * (kGraphH - 1));
            int32_t yB = kGraphY + (kGraphH - 1) -
                         static_cast<int32_t>((runtimeState_.mafHistory[i + 1] / maxSeen) * (kGraphH - 1));
            tft_.drawLine(xA, yA, xB, yB, theme_.primaryGaugeArc);
        }
    }
}

// ---------------------------------------------------------------- Config: UI --

void ClusterPages::drawConfigUiStatic() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);

    for (int i = 0; i <= 2; ++i) {
        int32_t y = layout::kHeaderHeight + i * layout::kConfigRowHeight;
        tft_.drawFastHLine(0, y, layout::kScreenWidth, theme_.bezel);
    }

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelActiveTheme, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Units row
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelUnits, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow1Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Gauge ticks toggle row
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelGaugeTicks, 12, layout::kConfigRow2Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow2Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Force drawConfigUiDynamic() to repaint every region the next time it runs
    runtimeState_.cfgUnitsDrawn[0] = '\0';
    runtimeState_.cfgThemeDrawn[0] = '\0';
    runtimeState_.cfgTicksDrawn[0] = '\0';
}

void ClusterPages::drawConfigUiDynamic(uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    char buf[32];

    if (strcmp(theme_.name, runtimeState_.cfgThemeDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(1);
        gaugewidgets::drawFieldText(tft_, theme_.name, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
                                     theme_.background);
        strncpy(runtimeState_.cfgThemeDrawn, theme_.name, sizeof(runtimeState_.cfgThemeDrawn) - 1);
    }

    snprintf(buf, sizeof(buf), "%s", settings.useMetricUnits ? labels::kLabelUnitsMetric : labels::kLabelUnitsStandard);
    if (strcmp(buf, runtimeState_.cfgUnitsDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(1);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow1Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
                                     theme_.background);
        strncpy(runtimeState_.cfgUnitsDrawn, buf, sizeof(runtimeState_.cfgUnitsDrawn) - 1);
    }

    const char* tickModeLabel;
    switch (static_cast<TickMode>(settings.tickMode)) {
        case TickMode::InsideOnly: tickModeLabel = labels::kLabelTickModeInsideOnly; break;
        case TickMode::OutsideOnly: tickModeLabel = labels::kLabelTickModeOutsideOnly; break;
        case TickMode::InsideAndOutside: tickModeLabel = labels::kLabelTickModeInsideAndOutside; break;
        case TickMode::Off:
        default: tickModeLabel = labels::kLabelTickModeOff; break;
    }
    snprintf(buf, sizeof(buf), "%s", tickModeLabel);
    if (strcmp(buf, runtimeState_.cfgTicksDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(1);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow2Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
                                     theme_.background);
        strncpy(runtimeState_.cfgTicksDrawn, buf, sizeof(runtimeState_.cfgTicksDrawn) - 1);
    }
}

// ---------------------------------------------------------------- Config: User Vars --

void ClusterPages::drawConfigUserVarsStatic() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);

    for (int i = 0; i <= 6; ++i) {
        int32_t y = layout::kHeaderHeight + i * layout::kConfigRowHeight;
        tft_.drawFastHLine(0, y, layout::kScreenWidth, theme_.bezel);
    }

    bool metric = configStore_.settings().useMetricUnits;

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelBoostBaroBaseline, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelCoolantWarningTemp, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelLowVoltageWarning, 12, layout::kConfigRow2Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelZeroSixtyTarget, 12, layout::kConfigRow3Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelHpEstimationFactor, 12, layout::kConfigRow4Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelFuelTrimRange, 12, layout::kConfigRow5Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    auto drawMinusPlusChrome = [&](int32_t rowY) {
        tft_.drawRoundRect(layout::kConfigMinusX, rowY + layout::kConfigButtonInsetY, layout::kConfigMinusW,
                            layout::kConfigButtonH, 4, theme_.bezel);
        tft_.drawRoundRect(layout::kConfigPlusX, rowY + layout::kConfigButtonInsetY, layout::kConfigPlusW,
                            layout::kConfigButtonH, 4, theme_.bezel);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        tft_.drawString(labels::kStepperMinus, layout::kConfigMinusX + layout::kConfigMinusW / 2,
                         rowY + layout::kConfigRowHeight / 2);
        tft_.drawString(labels::kStepperPlus, layout::kConfigPlusX + layout::kConfigPlusW / 2,
                         rowY + layout::kConfigRowHeight / 2);
    };
    drawMinusPlusChrome(layout::kConfigRow0Y);
    drawMinusPlusChrome(layout::kConfigRow1Y);
    drawMinusPlusChrome(layout::kConfigRow2Y);
    drawMinusPlusChrome(layout::kConfigRow3Y);
    drawMinusPlusChrome(layout::kConfigRow4Y);
    drawMinusPlusChrome(layout::kConfigRow5Y);

    // Fixed units, drawn once so they never shift or get redrawn when a
    // value's digit count changes; centered with a reserved value width so
    // each pair reads as centered instead of hugging the edge. Must match
    // the layout in drawConfigUserVarsDynamic(). HP factor (row4) is
    // unitless and has no fixed-unit call.
    auto drawFixedUnitForRow = [&](int32_t rowY, const char* unitLabel) {
        int32_t unitW = gaugewidgets::fixedUnitWidth(tft_, unitLabel, 2);
        int32_t valueW = gaugewidgets::fixedUnitWidth(tft_, "199.9", 2);
        gaugewidgets::ValueUnitGroup group = gaugewidgets::centerValueUnitGroup(
            layout::kConfigValueX + layout::kConfigValueW / 2, valueW, unitW, 4);
        gaugewidgets::drawFixedUnit(tft_, unitLabel, group.unitRightX, rowY + layout::kConfigRowHeight / 2, MR_DATUM,
                                     2, theme_, theme_.background);
    };
    drawFixedUnitForRow(layout::kConfigRow0Y, units::pressureUnitLabel(metric));
    drawFixedUnitForRow(layout::kConfigRow1Y, units::tempUnitLabel(metric));
    drawFixedUnitForRow(layout::kConfigRow2Y, "V");
    drawFixedUnitForRow(layout::kConfigRow3Y, units::speedUnitLabel(metric));
    drawFixedUnitForRow(layout::kConfigRow5Y, "%");

    // Force drawConfigUserVarsDynamic() to repaint every region the next time
    // it runs, since the static redraw above just wiped them all.
    runtimeState_.cfgBaroBaselineDrawn[0] = '\0';
    runtimeState_.cfgCoolantWarningDrawn[0] = '\0';
    runtimeState_.cfgLowVoltageWarningDrawn[0] = '\0';
    runtimeState_.cfgZeroSixtyTargetDrawn[0] = '\0';
    runtimeState_.cfgHpFactorDrawn[0] = '\0';
    runtimeState_.cfgFuelTrimRangeDrawn[0] = '\0';
}

void ClusterPages::drawConfigUserVarsDynamic(uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    char buf[24];

    // Row 0: Boost Baro Baseline.
    int32_t baselineUnitW = gaugewidgets::fixedUnitWidth(tft_, units::pressureUnitLabel(metric), 2);
    int32_t baselineValueW = gaugewidgets::fixedUnitWidth(tft_, "199.9", 2);
    gaugewidgets::ValueUnitGroup baselineGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, baselineValueW, baselineUnitW, 4);
    if (metric) {
        float kpa = units::kpaFromPsi(settings.baroBaselinePsi);
        snprintf(buf, sizeof(buf), "%.1f", kpa);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", settings.baroBaselinePsi);
    }
    if (strcmp(buf, runtimeState_.cfgBaroBaselineDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, baselineGroup.valueRightX,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, baselineValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgBaroBaselineDrawn, buf, sizeof(runtimeState_.cfgBaroBaselineDrawn) - 1);
    }

    // Row 1: Coolant Warning Temp.
    int32_t coolantUnitW = gaugewidgets::fixedUnitWidth(tft_, units::tempUnitLabel(metric), 2);
    int32_t coolantValueW = gaugewidgets::fixedUnitWidth(tft_, "199.9", 2);
    gaugewidgets::ValueUnitGroup coolantGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, coolantValueW, coolantUnitW, 4);
    if (metric) {
        snprintf(buf, sizeof(buf), "%.1f", units::celsiusFromFahrenheit(settings.coolantWarningF));
    } else {
        snprintf(buf, sizeof(buf), "%.0f", settings.coolantWarningF);
    }
    if (strcmp(buf, runtimeState_.cfgCoolantWarningDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, coolantGroup.valueRightX,
                                     layout::kConfigRow1Y + layout::kConfigRowHeight / 2, coolantValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgCoolantWarningDrawn, buf, sizeof(runtimeState_.cfgCoolantWarningDrawn) - 1);
    }

    // Row 2: Low Voltage Warning (no metric variant).
    int32_t voltUnitW = gaugewidgets::fixedUnitWidth(tft_, "V", 2);
    int32_t voltValueW = gaugewidgets::fixedUnitWidth(tft_, "199.9", 2);
    gaugewidgets::ValueUnitGroup voltGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, voltValueW, voltUnitW, 4);
    snprintf(buf, sizeof(buf), "%.1f", settings.lowVoltageWarningV);
    if (strcmp(buf, runtimeState_.cfgLowVoltageWarningDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, voltGroup.valueRightX,
                                     layout::kConfigRow2Y + layout::kConfigRowHeight / 2, voltValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgLowVoltageWarningDrawn, buf, sizeof(runtimeState_.cfgLowVoltageWarningDrawn) - 1);
    }

    // Row 3: 0-60 Target Speed.
    int32_t targetUnitW = gaugewidgets::fixedUnitWidth(tft_, units::speedUnitLabel(metric), 2);
    int32_t targetValueW = gaugewidgets::fixedUnitWidth(tft_, "199.9", 2);
    gaugewidgets::ValueUnitGroup targetGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, targetValueW, targetUnitW, 4);
    if (metric) {
        snprintf(buf, sizeof(buf), "%.0f", units::kphFromMph(settings.zeroSixtyTargetMph));
    } else {
        snprintf(buf, sizeof(buf), "%.0f", settings.zeroSixtyTargetMph);
    }
    if (strcmp(buf, runtimeState_.cfgZeroSixtyTargetDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, targetGroup.valueRightX,
                                     layout::kConfigRow3Y + layout::kConfigRowHeight / 2, targetValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgZeroSixtyTargetDrawn, buf, sizeof(runtimeState_.cfgZeroSixtyTargetDrawn) - 1);
    }

    // Row 4: HP Estimation Factor (unitless - plain centered value, no unit suffix).
    snprintf(buf, sizeof(buf), "%.2f", settings.hpEstimationFactor);
    if (strcmp(buf, runtimeState_.cfgHpFactorDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigValueX + layout::kConfigValueW / 2,
                                     layout::kConfigRow4Y + layout::kConfigRowHeight / 2, layout::kConfigValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgHpFactorDrawn, buf, sizeof(runtimeState_.cfgHpFactorDrawn) - 1);
    }

    // Row 5: Fuel Trim Display Range (no metric variant).
    int32_t trimUnitW = gaugewidgets::fixedUnitWidth(tft_, "%", 2);
    int32_t trimValueW = gaugewidgets::fixedUnitWidth(tft_, "199.9", 2);
    gaugewidgets::ValueUnitGroup trimGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, trimValueW, trimUnitW, 4);
    snprintf(buf, sizeof(buf), "%.0f", settings.fuelTrimRangePct);
    if (strcmp(buf, runtimeState_.cfgFuelTrimRangeDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, trimGroup.valueRightX,
                                     layout::kConfigRow5Y + layout::kConfigRowHeight / 2, trimValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgFuelTrimRangeDrawn, buf, sizeof(runtimeState_.cfgFuelTrimRangeDrawn) - 1);
    }
}

// ---------------------------------------------------------------- Config: Gauges --

void ClusterPages::drawConfigGaugesStatic() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);

    for (int i = 0; i <= 6; ++i) {
        int32_t y = layout::kHeaderHeight + i * layout::kConfigRowHeight;
        tft_.drawFastHLine(0, y, layout::kScreenWidth, theme_.bezel);
    }

    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelShiftLightRpm, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelRedlineRpm, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelMaxRpm, 12, layout::kConfigRow2Y + layout::kConfigRowHeight / 2);
    const char* maxSpeedLabel = metric ? labels::kLabelMaxSpeedKph : labels::kLabelMaxSpeedMph;
    tft_.drawString(maxSpeedLabel, 12, layout::kConfigRow3Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelVacuumGaugeMax, 12, layout::kConfigRow4Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelBoostGaugeMax, 12, layout::kConfigRow5Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    auto drawMinusPlusChrome = [&](int32_t rowY) {
        tft_.drawRoundRect(layout::kConfigMinusX, rowY + layout::kConfigButtonInsetY, layout::kConfigMinusW,
                            layout::kConfigButtonH, 4, theme_.bezel);
        tft_.drawRoundRect(layout::kConfigPlusX, rowY + layout::kConfigButtonInsetY, layout::kConfigPlusW,
                            layout::kConfigButtonH, 4, theme_.bezel);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        tft_.drawString(labels::kStepperMinus, layout::kConfigMinusX + layout::kConfigMinusW / 2, rowY + layout::kConfigRowHeight / 2);
        tft_.drawString(labels::kStepperPlus, layout::kConfigPlusX + layout::kConfigPlusW / 2, rowY + layout::kConfigRowHeight / 2);
    };
    drawMinusPlusChrome(layout::kConfigRow0Y);
    drawMinusPlusChrome(layout::kConfigRow1Y);
    drawMinusPlusChrome(layout::kConfigRow2Y);
    drawMinusPlusChrome(layout::kConfigRow3Y);
    drawMinusPlusChrome(layout::kConfigRow4Y);
    drawMinusPlusChrome(layout::kConfigRow5Y);

    // Fixed units for the two new gauge-max rows; the four RPM/speed rows
    // above have no unit suffix.
    auto drawFixedUnitForRow = [&](int32_t rowY, const char* unitLabel) {
        int32_t unitW = gaugewidgets::fixedUnitWidth(tft_, unitLabel, 2);
        int32_t valueW = gaugewidgets::fixedUnitWidth(tft_, "999", 2);
        gaugewidgets::ValueUnitGroup group = gaugewidgets::centerValueUnitGroup(
            layout::kConfigValueX + layout::kConfigValueW / 2, valueW, unitW, 4);
        gaugewidgets::drawFixedUnit(tft_, unitLabel, group.unitRightX, rowY + layout::kConfigRowHeight / 2, MR_DATUM,
                                     2, theme_, theme_.background);
    };
    drawFixedUnitForRow(layout::kConfigRow4Y, metric ? labels::kUnitKpa : labels::kUnitInHg);
    drawFixedUnitForRow(layout::kConfigRow5Y, units::pressureUnitLabel(metric));

    // Force drawConfigGaugesDynamic() to repaint every region the next time
    // it runs, since the static redraw above just wiped them all.
    runtimeState_.cfgShiftLightRpmDrawn[0] = '\0';
    runtimeState_.cfgRedlineRpmDrawn[0] = '\0';
    runtimeState_.cfgMaxRpmDrawn[0] = '\0';
    runtimeState_.cfgMaxSpeedDrawn[0] = '\0';
    runtimeState_.cfgBoostMaxDrawn[0] = '\0';
    runtimeState_.cfgVacuumMaxDrawn[0] = '\0';
}

void ClusterPages::drawConfigGaugesDynamic(uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    char buf[24];

    snprintf(buf, sizeof(buf), "%u", settings.shiftLightRpm);
    if (strcmp(buf, runtimeState_.cfgShiftLightRpmDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigValueX + layout::kConfigValueW / 2,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, layout::kConfigValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgShiftLightRpmDrawn, buf, sizeof(runtimeState_.cfgShiftLightRpmDrawn) - 1);
    }

    snprintf(buf, sizeof(buf), "%u", settings.redlineRpm);
    if (strcmp(buf, runtimeState_.cfgRedlineRpmDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigValueX + layout::kConfigValueW / 2,
                                     layout::kConfigRow1Y + layout::kConfigRowHeight / 2, layout::kConfigValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgRedlineRpmDrawn, buf, sizeof(runtimeState_.cfgRedlineRpmDrawn) - 1);
    }

    snprintf(buf, sizeof(buf), "%u", settings.maxRpm);
    if (strcmp(buf, runtimeState_.cfgMaxRpmDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigValueX + layout::kConfigValueW / 2,
                                     layout::kConfigRow2Y + layout::kConfigRowHeight / 2, layout::kConfigValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgMaxRpmDrawn, buf, sizeof(runtimeState_.cfgMaxRpmDrawn) - 1);
    }

    bool metric = settings.useMetricUnits;
    if (metric) {
        snprintf(buf, sizeof(buf), "%u", static_cast<uint16_t>(units::kphFromMph(settings.maxSpeedMph)));
    } else {
        snprintf(buf, sizeof(buf), "%u", settings.maxSpeedMph);
    }
    if (strcmp(buf, runtimeState_.cfgMaxSpeedDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigValueX + layout::kConfigValueW / 2,
                                     layout::kConfigRow3Y + layout::kConfigRowHeight / 2, layout::kConfigValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgMaxSpeedDrawn, buf, sizeof(runtimeState_.cfgMaxSpeedDrawn) - 1);
    }

    // Row 4: Vacuum Gauge Max.
    int32_t vacUnitW = gaugewidgets::fixedUnitWidth(tft_, metric ? labels::kUnitKpa : labels::kUnitInHg, 2);
    int32_t vacValueW = gaugewidgets::fixedUnitWidth(tft_, "999", 2);
    gaugewidgets::ValueUnitGroup vacGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, vacValueW, vacUnitW, 4);
    if (metric) {
        snprintf(buf, sizeof(buf), "%.0f", units::kpaFromInHg(settings.vacuumMaxInHg));
    } else {
        snprintf(buf, sizeof(buf), "%.0f", settings.vacuumMaxInHg);
    }
    if (strcmp(buf, runtimeState_.cfgVacuumMaxDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, vacGroup.valueRightX,
                                     layout::kConfigRow4Y + layout::kConfigRowHeight / 2, vacValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgVacuumMaxDrawn, buf, sizeof(runtimeState_.cfgVacuumMaxDrawn) - 1);
    }

    // Row 5: Boost Gauge Max.
    int32_t boostUnitW = gaugewidgets::fixedUnitWidth(tft_, units::pressureUnitLabel(metric), 2);
    int32_t boostValueW = gaugewidgets::fixedUnitWidth(tft_, "999", 2);
    gaugewidgets::ValueUnitGroup boostGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigValueX + layout::kConfigValueW / 2, boostValueW, boostUnitW, 4);
    if (metric) {
        snprintf(buf, sizeof(buf), "%.0f", units::kpaFromPsi(settings.boostMaxPsi));
    } else {
        snprintf(buf, sizeof(buf), "%.0f", settings.boostMaxPsi);
    }
    if (strcmp(buf, runtimeState_.cfgBoostMaxDrawn) != 0) {
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, boostGroup.valueRightX,
                                     layout::kConfigRow5Y + layout::kConfigRowHeight / 2, boostValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgBoostMaxDrawn, buf, sizeof(runtimeState_.cfgBoostMaxDrawn) - 1);
    }
}

// ---------------------------------------------------------------- Config: Logs --

void ClusterPages::drawConfigLogsStatic() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);

    // Log Units + its warning form a taller section than the standard 40px
    // row, so the grid lines below aren't evenly spaced like the loop-based
    // approach other config pages use.
    tft_.drawFastHLine(0, layout::kConfigRow0Y, layout::kScreenWidth, theme_.bezel);
    tft_.drawFastHLine(0, layout::kConfigRow1Y, layout::kScreenWidth, theme_.bezel);
    tft_.drawFastHLine(0, layout::kLogsSummaryRowY, layout::kScreenWidth, theme_.bezel);
    tft_.drawFastHLine(0, layout::kLogsSummaryRowY + layout::kConfigRowHeight, layout::kScreenWidth, theme_.bezel);

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelLogInterval, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Fixed unit, drawn once so it never shifts or gets redrawn when the
    // interval's digit count changes; centered with a reserved value width
    // so the pair reads as centered instead of hugging the edge. Must match
    // drawConfigLogsDynamic().
    int32_t intervalUnitW = gaugewidgets::fixedUnitWidth(tft_, "ms", 2);
    int32_t intervalValueW = gaugewidgets::fixedUnitWidth(tft_, "99999", 2);
    gaugewidgets::ValueUnitGroup intervalGroup = gaugewidgets::centerValueUnitGroup(
        layout::kConfigCycleX + layout::kConfigCycleW / 2, intervalValueW, intervalUnitW, 4);
    gaugewidgets::drawFixedUnit(tft_, "ms", intervalGroup.unitRightX,
                                 layout::kConfigRow0Y + layout::kConfigRowHeight / 2, MR_DATUM, 2, theme_,
                                 theme_.background);

    // Fixed "LOGS,"/"MB" captions for the log summary line below: the file
    // count and total size each get a fixed-width slot so neither shifts the
    // words around them as their digit counts change; must match the layout
    // in drawConfigLogsDynamic().
    constexpr int32_t kLogsCountSlotW = 40; // room for up to 4-digit file counts
    constexpr int32_t kLogsSizeSlotW = 48;  // room for up to "9999.9"
    int32_t logsRowMidY = layout::kLogsSummaryRowY + layout::kConfigRowHeight / 2;
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    tft_.setTextSize(1);
    tft_.setTextFont(1);
    int32_t logsLabelX = 12 + kLogsCountSlotW + 4;
    tft_.drawString("LOGS,", logsLabelX, logsRowMidY);
    int32_t logsSizeX = logsLabelX + tft_.textWidth("LOGS,") + 4;
    tft_.drawString("MB", logsSizeX + kLogsSizeSlotW + 4, logsRowMidY);

    // Log Units row (row 1), with the warning drawn directly below it
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelLogUnits, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);
    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow1Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Warning text, left-aligned directly under the Log Units label — drawn
    // static since it never changes
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.warningActive, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kWarningLogUnitsDeletesLogs, 12, layout::kConfigRow1Y + layout::kConfigRowHeight + 6);

    // Force drawConfigLogsDynamic() to repaint every region the next time it
    // runs, since the static redraw above just wiped them all.
    runtimeState_.cfgLogIntervalDrawn[0] = '\0';
    runtimeState_.cfgLogUnitsDrawn[0] = '\0';
    runtimeState_.cfgDeleteConfirmDrawn = -1;
    runtimeState_.cfgLogSummaryDrawn[0] = '\0';
    runtimeState_.cfgLogSummaryNextScanMs = 0;
}

void ClusterPages::drawConfigLogsDynamic(uint32_t nowMs) {
    const AppSettings& settings = configStore_.settings();
    char buf[24];

    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(settings.logIntervalMs));
    if (strcmp(buf, runtimeState_.cfgLogIntervalDrawn) != 0) {
        int32_t intervalUnitW = gaugewidgets::fixedUnitWidth(tft_, "ms", 2);
        int32_t intervalValueW = gaugewidgets::fixedUnitWidth(tft_, "99999", 2);
        gaugewidgets::ValueUnitGroup intervalGroup = gaugewidgets::centerValueUnitGroup(
            layout::kConfigCycleX + layout::kConfigCycleW / 2, intervalValueW, intervalUnitW, 4);
        tft_.setTextDatum(MR_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, intervalGroup.valueRightX,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, intervalValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgLogIntervalDrawn, buf, sizeof(runtimeState_.cfgLogIntervalDrawn) - 1);
    }

    // Log Units row (row 1)
    snprintf(buf, sizeof(buf), "%s", settings.useMetricLogs ? labels::kLabelUnitsMetric : labels::kLabelUnitsStandard);
    if (strcmp(buf, runtimeState_.cfgLogUnitsDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(1);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow1Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
                                     theme_.background);
        strncpy(runtimeState_.cfgLogUnitsDrawn, buf, sizeof(runtimeState_.cfgLogUnitsDrawn) - 1);
    }

    // getLogSummary() scans the SD card's log directory, so it's only
    // rescanned periodically rather than on every UI refresh tick.
    if (nowMs >= runtimeState_.cfgLogSummaryNextScanMs) {
        constexpr uint32_t kLogSummaryScanIntervalMs = 2000;
        runtimeState_.cfgLogSummaryNextScanMs = nowMs + kLogSummaryScanIntervalMs;

        LogSummary summary = csvLogger_.getLogSummary();
        float totalMb = static_cast<float>(summary.totalBytes) / (1024.0F * 1024.0F);
        snprintf(buf, sizeof(buf), "%lu|%.1f", static_cast<unsigned long>(summary.fileCount), totalMb);
        if (strcmp(buf, runtimeState_.cfgLogSummaryDrawn) != 0) {
            // Matches the fixed "LOGS,"/"MB" captions drawn once by
            // drawConfigLogsStatic() — see its comment for the slot layout.
            constexpr int32_t kLogsCountSlotW = 40;
            constexpr int32_t kLogsSizeSlotW = 48;
            int32_t logsRowMidY = layout::kLogsSummaryRowY + layout::kConfigRowHeight / 2;
            char countBuf[8];
            char sizeBuf[12];
            snprintf(countBuf, sizeof(countBuf), "%lu", static_cast<unsigned long>(summary.fileCount));
            snprintf(sizeBuf, sizeof(sizeBuf), "%.1f", totalMb);
            tft_.setTextDatum(ML_DATUM);
            tft_.setTextColor(theme_.textPrimary, theme_.background);
            tft_.setTextSize(1);
            gaugewidgets::drawFieldText(tft_, countBuf, 12, logsRowMidY, kLogsCountSlotW, theme_.background);
            int32_t logsLabelX = 12 + kLogsCountSlotW + 4;
            int32_t logsSizeX = logsLabelX + tft_.textWidth("LOGS,") + 4;
            gaugewidgets::drawFieldText(tft_, sizeBuf, logsSizeX, logsRowMidY, kLogsSizeSlotW, theme_.background);
            strncpy(runtimeState_.cfgLogSummaryDrawn, buf, sizeof(runtimeState_.cfgLogSummaryDrawn) - 1);
        }
    }

    bool confirmArmed =
        runtimeState_.deleteLogsConfirmArmed && (nowMs - runtimeState_.deleteLogsConfirmArmedAtMs < 5000);
    if (runtimeState_.deleteLogsConfirmArmed && !confirmArmed) {
        runtimeState_.deleteLogsConfirmArmed = false;
    }
    if (runtimeState_.cfgDeleteConfirmDrawn != static_cast<int8_t>(confirmArmed)) {
        uint16_t deleteColor = confirmArmed ? theme_.warningActive : theme_.panel;
        tft_.fillRoundRect(layout::kConfigDeleteX, layout::kLogsSummaryRowY + layout::kConfigButtonInsetY,
                            layout::kConfigDeleteW, layout::kConfigButtonH, 6, deleteColor);
        tft_.drawRoundRect(layout::kConfigDeleteX, layout::kLogsSummaryRowY + layout::kConfigButtonInsetY,
                            layout::kConfigDeleteW, layout::kConfigButtonH, 6, theme_.bezel);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextSize(1);
        tft_.setTextColor(confirmArmed ? theme_.background : theme_.textPrimary, deleteColor);
        tft_.drawString(confirmArmed ? labels::kButtonTapToConfirm : labels::kButtonDeleteAllLogs,
                         layout::kConfigDeleteX + layout::kConfigDeleteW / 2,
                         layout::kLogsSummaryRowY + layout::kConfigRowHeight / 2);
        runtimeState_.cfgDeleteConfirmDrawn = static_cast<int8_t>(confirmArmed);
    }
}

// ---------------------------------------------------------------- Config: OBD Adapter --

void ClusterPages::drawConfigObdStatic() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);

    tft_.drawFastHLine(0, layout::kConfigRow0Y, layout::kScreenWidth, theme_.bezel);
    tft_.drawFastHLine(0, layout::kConfigRow1Y, layout::kScreenWidth, theme_.bezel);
    tft_.drawFastHLine(0, layout::kConfigRow1Y + layout::kConfigRowHeight, layout::kScreenWidth, theme_.bezel);

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    applyLabelFont(tft_);
    tft_.drawString(labels::kLabelObdAdapterName, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelObdAdapterPin, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
    resetValueFont(tft_);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);
    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow1Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Informational hint, not a warning - drawn static since it never changes.
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kHintObdReconnectOnSave, layout::kScreenWidth / 2, layout::kConfigRow1Y + layout::kConfigRowHeight + 24);

    // Force drawConfigObdDynamic() to repaint every region the next time it
    // runs, since the static redraw above just wiped them all.
    runtimeState_.cfgObdAdapterNameDrawn[0] = '\0';
    runtimeState_.cfgObdAdapterPinDrawn[0] = '\0';
}

void ClusterPages::drawConfigObdDynamic(uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();

    if (strcmp(settings.obdAdapterName, runtimeState_.cfgObdAdapterNameDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, settings.obdAdapterName, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
                                     theme_.background);
        strncpy(runtimeState_.cfgObdAdapterNameDrawn, settings.obdAdapterName,
                sizeof(runtimeState_.cfgObdAdapterNameDrawn) - 1);
    }

    if (strcmp(settings.obdAdapterPin, runtimeState_.cfgObdAdapterPinDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, settings.obdAdapterPin, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow1Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
                                     theme_.background);
        strncpy(runtimeState_.cfgObdAdapterPinDrawn, settings.obdAdapterPin,
                sizeof(runtimeState_.cfgObdAdapterPinDrawn) - 1);
    }
}

// ---------------------------------------------------------------- Config: shared footer --

void ClusterPages::drawConfigFooterStatic() {
    tft_.drawFastHLine(0, layout::kConfigFooterY, layout::kScreenWidth, theme_.bezel);

    // Force drawConfigFooterDynamic() to repaint the button the next time it
    // runs, since a page switch may have just cleared this area.
    runtimeState_.cfgSaveButtonDrawn = -1;
}

void ClusterPages::drawConfigFooterDynamic(uint32_t nowMs) {
    bool showSavedFeedback = runtimeState_.configStatusMessage[0] != '\0' &&
                              (nowMs - runtimeState_.configStatusMessageSetAtMs < 1500);
    if (runtimeState_.configStatusMessage[0] != '\0' && !showSavedFeedback) {
        runtimeState_.configStatusMessage[0] = '\0';
    }

    // 0 = clean (default color), 1 = unsaved changes (green), 2 = "SAVED!"
    // feedback (default color) - tracked as one state so a color change and a
    // text change are never missed independently of each other.
    int8_t visualState = showSavedFeedback ? 2 : (configStore_.isDirty() ? 1 : 0);
    if (runtimeState_.cfgSaveButtonDrawn != visualState) {
        uint16_t fillColor = (visualState == 1) ? theme_.unsavedActive : theme_.primaryGaugeArc;
        tft_.fillRoundRect(layout::kConfigSaveX, layout::kConfigFooterY + layout::kConfigButtonInsetY,
                            layout::kConfigSaveW, layout::kConfigButtonH, 6, fillColor);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextSize(2);
        tft_.setTextColor(theme_.background, fillColor);
        tft_.drawString(showSavedFeedback ? labels::kButtonSaved : labels::kButtonSaveToSd, layout::kScreenWidth / 2,
                         layout::kConfigFooterY + layout::kConfigRowHeight / 2);
        runtimeState_.cfgSaveButtonDrawn = visualState;
    }
}

// ---------------------------------------------------------------- Page 6: Diagnostics --

namespace {

bool dtcListsEqual(const DtcList& a, const DtcList& b) {
    if (a.count != b.count) {
        return false;
    }
    for (uint8_t i = 0; i < a.count; ++i) {
        if (strcmp(a.codes[i], b.codes[i]) != 0) {
            return false;
        }
    }
    return true;
}

} // namespace

void ClusterPages::drawPage6Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    tft_.drawRoundRect(20, layout::kDtcListY - 10, 440,
                        layout::kDtcListLineHeight * layout::kDtcListVisibleLines + 20, 6, theme_.bezel);

    // REFRESH CODES never changes appearance, so it only needs to be drawn
    // once here rather than every drawPage6Dynamic() tick.
    tft_.fillRoundRect(layout::kDtcReadButtonX, layout::kDtcButtonY, layout::kDtcReadButtonW, layout::kDtcButtonH, 6,
                        theme_.primaryGaugeArc);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.background, theme_.primaryGaugeArc);
    tft_.setTextSize(1);
    tft_.drawString(labels::kButtonRefreshCodes, layout::kDtcReadButtonX + layout::kDtcReadButtonW / 2,
                     layout::kDtcButtonY + layout::kDtcButtonH / 2);

    // Force drawPage6Dynamic() to repaint every region the next time it
    // runs, since the static redraw above just wiped them all.
    runtimeState_.dtcMilOnDrawn = -1;
    runtimeState_.dtcHaveResultDrawn = -1;
    runtimeState_.dtcListDrawn = DtcList();
    runtimeState_.dtcClearConfirmDrawn = -1;

    // Kick off a fresh DTC read every time this page is opened.
    obdClient_.requestDtcRead();
}

void ClusterPages::drawPage6Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    bool milOn = snapshot.milOn.valid && snapshot.milOn.value != 0.0F;
    if (runtimeState_.dtcMilOnDrawn != static_cast<int8_t>(milOn)) {
        tft_.setTextDatum(TC_DATUM);
        tft_.setTextColor(milOn ? theme_.warningActive : theme_.textPrimary, theme_.background);
        applyValueFont(tft_, theme_, 2);
        gaugewidgets::drawFieldText(tft_, milOn ? labels::kStatusMilActive : labels::kStatusMilInactive,
                                     layout::kScreenWidth / 2, 46, 300, theme_.background);
        resetValueFont(tft_);
        runtimeState_.dtcMilOnDrawn = static_cast<int8_t>(milOn);
    }

    DtcList dtcList;
    obdClient_.getDtcList(dtcList);
    bool haveResult = obdClient_.hasDtcResult();

    bool dtcListChanged = runtimeState_.dtcHaveResultDrawn != static_cast<int8_t>(haveResult) ||
                           (haveResult && !dtcListsEqual(dtcList, runtimeState_.dtcListDrawn));
    if (dtcListChanged) {
        applyValueFont(tft_, theme_, 2);
        // Clear the whole box every time regardless of how many lines will actually be
        // shown, so a shrinking list (fewer codes, or a fresh read) never leaves a stale
        // line from a taller previous draw.
        for (uint8_t line = 0; line < layout::kDtcListVisibleLines; ++line) {
            int32_t y = layout::kDtcListY + line * layout::kDtcListLineHeight;
            tft_.fillRect(30, y, 420, layout::kDtcListLineHeight - 2, theme_.background);
        }

        // Center the actual content block vertically within the box's line span, and
        // each line horizontally on the box's x-center (matches the drawRoundRect box in
        // drawPage6Static: x 20..460).
        uint8_t contentLines = 1;
        if (haveResult && dtcList.count > 0) {
            contentLines = dtcList.count < layout::kDtcListVisibleLines ? dtcList.count : layout::kDtcListVisibleLines;
        }
        int32_t startY = layout::kDtcListY + (layout::kDtcListVisibleLines - contentLines) * layout::kDtcListLineHeight / 2;
        constexpr int32_t kDtcCenterX = 20 + 440 / 2;

        tft_.setTextDatum(MC_DATUM);
        if (!haveResult) {
            tft_.setTextColor(theme_.textSecondary, theme_.background);
            tft_.drawString(labels::kStatusReadingCodes, kDtcCenterX, startY + layout::kDtcListLineHeight / 2);
        } else if (dtcList.count == 0) {
            tft_.setTextColor(theme_.textSecondary, theme_.background);
            tft_.drawString(labels::kStatusNoCodes, kDtcCenterX, startY + layout::kDtcListLineHeight / 2);
        } else {
            tft_.setTextColor(theme_.textPrimary, theme_.background);
            for (uint8_t line = 0; line < contentLines; ++line) {
                int32_t y = startY + line * layout::kDtcListLineHeight + layout::kDtcListLineHeight / 2;
                tft_.drawString(dtcList.codes[line], kDtcCenterX, y);
            }
        }
        resetValueFont(tft_);
        runtimeState_.dtcHaveResultDrawn = static_cast<int8_t>(haveResult);
        runtimeState_.dtcListDrawn = dtcList;
    }

    bool confirmArmed =
        runtimeState_.clearCodesConfirmArmed && (nowMs - runtimeState_.clearCodesConfirmArmedAtMs < 5000);
    if (runtimeState_.clearCodesConfirmArmed && !confirmArmed) {
        runtimeState_.clearCodesConfirmArmed = false;
    }

    if (runtimeState_.dtcClearConfirmDrawn != static_cast<int8_t>(confirmArmed)) {
        uint16_t clearColor = confirmArmed ? theme_.warningActive : theme_.panel;
        tft_.fillRoundRect(layout::kDtcClearButtonX, layout::kDtcButtonY, layout::kDtcClearButtonW,
                            layout::kDtcButtonH, 6, clearColor);
        tft_.drawRoundRect(layout::kDtcClearButtonX, layout::kDtcButtonY, layout::kDtcClearButtonW,
                            layout::kDtcButtonH, 6, theme_.bezel);
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextSize(1);
        tft_.setTextColor(confirmArmed ? theme_.background : theme_.textPrimary, clearColor);
        tft_.drawString(confirmArmed ? labels::kButtonTapToConfirm : labels::kButtonClearCodes,
                         layout::kDtcClearButtonX + layout::kDtcClearButtonW / 2,
                         layout::kDtcButtonY + layout::kDtcButtonH / 2);
        runtimeState_.dtcClearConfirmDrawn = static_cast<int8_t>(confirmArmed);
    }
}
