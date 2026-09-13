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

void ClusterPages::drawStatusStrip(ConnectionState connectionState, bool sdLoggingActive) {
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

    constexpr uint16_t kSdActiveColor = 0x07E0;   // green: SD logging active
    constexpr uint16_t kSdInactiveColor = 0x0320; // dark green: SD logging inactive
    uint16_t sdColor = sdLoggingActive ? kSdActiveColor : kSdInactiveColor;
    tft_.fillCircle(layout::kSdLightCenterX, layout::kSdLightCenterY, layout::kSdLightRadius, sdColor);
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
                                ConnectionState connectionState, bool sdLoggingActive, uint32_t nowMs) {
    bool milOn = snapshot.milOn.valid && snapshot.milOn.value != 0.0F;
    if (runtimeState_.headerMilOnDrawn != static_cast<int8_t>(milOn)) {
        gaugewidgets::drawMilIndicator(tft_, layout::kMilCenterX, layout::kMilCenterY, milOn, theme_);
        runtimeState_.headerMilOnDrawn = static_cast<int8_t>(milOn);
    }
    drawStatusStrip(connectionState, sdLoggingActive);

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
        bool metric = configStore_.settings().useMetricUnits;
        float threshold = metric ? units::mphFromKph(100.0F) : 60.0F;
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

    if (settings.showGaugeTicks) {
        gaugewidgets::drawGaugeTicks(tft_, kRpmGaugeCx, kGaugeCy, kGaugeRadius, rpmNeedle_.currentValue, kRpmMax,
                                      config::kRpmTickIntervalMinor, config::kRpmTickIntervalMajor,
                                      static_cast<float>(settings.redlineRpm), theme_);
        gaugewidgets::drawGaugeTicks(tft_, kSpeedGaugeCx, kGaugeCy, kGaugeRadius, speedNeedle_.currentValue,
                                      kSpeedMax, config::kSpeedTickIntervalMinor, config::kSpeedTickIntervalMajor,
                                      kSpeedMax, theme_);
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
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(rpmTextColor, theme_.background);
    beginLargeText(4);
    // Font 7 digits are 32px wide each at size 1, so 4 digits (up to maxRpm=9000) need
    // 128px - wider than the 120px field sized for the default font. A field narrower
    // than the actual text leaves part of a shrinking digit uncleared on the next frame.
    int32_t rpmFieldWidth = theme_.useSevenSegmentFont ? 132 : 120;
    gaugewidgets::drawFieldText(tft_, rpmBuf, kRpmGaugeCx, kGaugeCy - 5, rpmFieldWidth, theme_.background);
    endLargeText();
    tft_.setTextSize(2);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.drawString(labels::kUnitRpm, kRpmGaugeCx, kGaugeCy + 30);

    char speedBuf[8];
    if (snapshot.speedMph.valid) {
        snprintf(speedBuf, sizeof(speedBuf), "%d", static_cast<int>(speedNeedle_.currentValue));
    } else {
        strcpy(speedBuf, "--");
    }
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    beginLargeText(4);
    gaugewidgets::drawFieldText(tft_, speedBuf, kSpeedGaugeCx, kGaugeCy - 5, 120, theme_.background);
    endLargeText();
    tft_.setTextSize(2);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.drawString(units::speedUnitLabel(metric), kSpeedGaugeCx, kGaugeCy + 30);

    constexpr int32_t kRowY = 250, kColW = 154, kColGap = 5;
    char valueBuf[16];

    bool coolantHot = snapshot.coolantF.valid && snapshot.coolantF.value > config::kHighCoolantWarningF;
    float displayCoolant = units::displayTemp(snapshot.coolantF.value, metric);
    snprintf(valueBuf, sizeof(valueBuf), "%d %s", static_cast<int>(displayCoolant), units::tempUnitLabel(metric));
    int32_t coolantColX = 4 + 0 * (kColW + kColGap);
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelCoolant, coolantColX + kColW / 2, kRowY + 8);
    tft_.setTextColor(coolantHot ? theme_.warningActive
                                  : (snapshot.coolantF.valid ? theme_.textPrimary : theme_.textSecondary),
                       theme_.panel);
    tft_.setTextSize(2);
    gaugewidgets::drawFieldText(tft_, snapshot.coolantF.valid ? valueBuf : "--", coolantColX + kColW / 2, kRowY + 22,
                                 kColW - 4, theme_.panel);

    float displayIat = units::displayTemp(snapshot.iatF.value, metric);
    snprintf(valueBuf, sizeof(valueBuf), "%d %s", static_cast<int>(displayIat), units::tempUnitLabel(metric));
    gaugewidgets::drawValueBox(tft_, 4 + 1 * (kColW + kColGap), kRowY + 8, kColW, labels::kLabelIat, valueBuf,
                                snapshot.iatF.valid, theme_);

    int32_t throttleColX = 4 + 2 * (kColW + kColGap);
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelThrottle, throttleColX + kColW / 2, kRowY + 8);
    gaugewidgets::drawBarGauge(tft_, throttleBar_, throttleColX + 8, kRowY + 24, kColW - 16, 24,
                                snapshot.throttlePct.valid ? snapshot.throttlePct.value : 0.0F,
                                theme_.primaryGaugeArc, theme_);
}

// ---------------------------------------------------------------- Page 2: Engine Load & Airflow --

void ClusterPages::drawPage2Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    loadArc_.invalidate();
    stftBar_.invalidate();
    ltftBar_.invalidate();

    // Must match the Engine Load gauge geometry in drawPage2Dynamic().
    constexpr int32_t kGaugeCx = 130, kGaugeCy = 130, kGaugeRadius = 78;
    gaugewidgets::drawGaugeBezel(tft_, kGaugeCx, kGaugeCy, kGaugeRadius, theme_);

    tft_.fillRoundRect(250, 50, 220, 160, 6, theme_.panel); // MAF + TIMING ADVANCE box, 20px taller
    tft_.fillRoundRect(20, 220, 210, 80, 6, theme_.panel);
    tft_.fillRoundRect(250, 220, 210, 80, 6, theme_.panel);
}

void ClusterPages::drawPage2Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    (void)nowMs;
    constexpr int32_t kGaugeCx = 130, kGaugeCy = 130, kGaugeRadius = 78;
    char buf[24];

    float targetLoad = snapshot.engineLoadPct.valid ? snapshot.engineLoadPct.value : 0.0F;
    gaugewidgets::drawArcGauge(tft_, loadArc_, kGaugeCx, kGaugeCy, kGaugeRadius, targetLoad, 100.0F, 100.0F, 100.0F,
                                theme_.background, theme_);

    tft_.setTextDatum(MC_DATUM);
    if (theme_.useSevenSegmentFont) {
        snprintf(buf, sizeof(buf), "%d", static_cast<int>(targetLoad)); // Font 7 has no '%' glyph.
    } else {
        snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(targetLoad));
    }
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    beginLargeText(3);
    gaugewidgets::drawFieldText(tft_, snapshot.engineLoadPct.valid ? buf : "--", kGaugeCx, kGaugeCy, 100,
                                 theme_.background);
    endLargeText();
    tft_.setTextSize(1);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.drawString(labels::kLabelEngineLoad, kGaugeCx, kGaugeCy + 34);

    if (snapshot.mafGps.valid && snapshot.mafGps.value > runtimeState_.mafPeakGps) {
        runtimeState_.mafPeakGps = snapshot.mafGps.value;
    }
    snprintf(buf, sizeof(buf), "%.1f g/s", snapshot.mafGps.value);
    gaugewidgets::drawValueBox(tft_, 260, 58, 200, labels::kLabelMaf, buf, snapshot.mafGps.valid, theme_, 2, 2);
    char peakBuf[24];
    snprintf(peakBuf, sizeof(peakBuf), "PEAK %.1f g/s", runtimeState_.mafPeakGps);
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    gaugewidgets::drawFieldText(tft_, peakBuf, 360, 105, 200, theme_.panel);

    snprintf(buf, sizeof(buf), "%.1f deg", snapshot.timingAdvanceDeg.value);
    gaugewidgets::drawValueBox(tft_, 260, 132, 200, labels::kLabelTimingAdvance, buf, snapshot.timingAdvanceDeg.valid, theme_, 2, 2);

    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelStftBank1, 125, 228);
    // Map -25%..+25% onto the 0-100% bar widget so the fill visually centers.
    float stftCentered = snapshot.stftPct.valid ? (snapshot.stftPct.value + 25.0F) * 2.0F : 0.0F;
    gaugewidgets::drawBarGauge(tft_, stftBar_, 30, 250, 190, 30, stftCentered, theme_.primaryGaugeArc, theme_);
    snprintf(buf, sizeof(buf), "%.1f%%", snapshot.stftPct.value);
    tft_.setTextColor(theme_.textPrimary, theme_.panel);
    gaugewidgets::drawFieldText(tft_, snapshot.stftPct.valid ? buf : "--", 125, 288, 190, theme_.panel);

    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.drawString(labels::kLabelLtftBank1, 355, 228);
    float ltftCentered = snapshot.ltftPct.valid ? (snapshot.ltftPct.value + 25.0F) * 2.0F : 0.0F;
    gaugewidgets::drawBarGauge(tft_, ltftBar_, 260, 250, 190, 30, ltftCentered, theme_.primaryGaugeArc, theme_);
    snprintf(buf, sizeof(buf), "%.1f%%", snapshot.ltftPct.value);
    tft_.setTextColor(theme_.textPrimary, theme_.panel);
    gaugewidgets::drawFieldText(tft_, snapshot.ltftPct.valid ? buf : "--", 355, 288, 190, theme_.panel);
}

// ---------------------------------------------------------------- Page 3: Car-Specific Sensors --

void ClusterPages::drawPage3Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    vacuumArc_.invalidate();

    // Must match the Vacuum/Boost gauge geometry in drawPage3Dynamic().
    constexpr int32_t kVacCx = 240, kVacCy = 211, kVacR = 82;
    gaugewidgets::drawGaugeBezel(tft_, kVacCx, kVacCy, kVacR, theme_);

    tft_.fillRoundRect(10, 50, 225, 70, 6, theme_.panel);
    tft_.fillRoundRect(245, 50, 225, 70, 6, theme_.panel);
    tft_.fillRoundRect(10, 245, 140, 65, 6, theme_.panel);
    tft_.fillRoundRect(330, 245, 140, 65, 6, theme_.panel);
}

void ClusterPages::drawPage3Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    char buf[20];

    bool lowVoltage = snapshot.voltageV.valid && snapshot.voltageV.value < config::kLowVoltageWarningV;
    snprintf(buf, sizeof(buf), "%.2f V", snapshot.voltageV.value);
    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelBatteryVoltage, 122, 58);
    tft_.setTextColor(lowVoltage ? theme_.warningActive
                                  : (snapshot.voltageV.valid ? theme_.textPrimary : theme_.textSecondary),
                       theme_.panel);
    tft_.setTextSize(2);
    gaugewidgets::drawFieldText(tft_, snapshot.voltageV.valid ? buf : "--", 122, 80, 200, theme_.panel);

    float displayFuelPressure = units::displayPressureFromKpa(snapshot.fuelPressureKpa.value, metric);
    snprintf(buf, sizeof(buf), "%d %s", static_cast<int>(displayFuelPressure), units::pressureUnitLabel(metric));
    gaugewidgets::drawValueBox(tft_, 245, 58, 225, labels::kLabelFuelRailPressure, buf, snapshot.fuelPressureKpa.valid, theme_);

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
                gaugeMax = 172.0F; // ~25 psi equivalent in kPa
            } else {
                gaugeValue = units::psiFromKpa(mapKpa - baroBaselineKpa);
                gaugeMax = 25.0F;
            }
            gaugeLabel = labels::kLabelBoost;
            gaugeUnit = metric ? labels::kUnitKpa : labels::kUnitPsi;
        } else {
            if (metric) {
                gaugeValue = baroBaselineKpa - mapKpa;
                gaugeMax = 101.0F; // ~30 inHg equivalent in kPa
            } else {
                gaugeValue = units::inHgFromKpa(baroBaselineKpa - mapKpa);
                gaugeMax = 30.0F;
            }
            gaugeLabel = labels::kLabelVacuum;
            gaugeUnit = metric ? labels::kUnitKpa : labels::kUnitInHg;
        }
    }
    // Sits in the gap between the two bottom panels (x 150..330), so it can run
    // lower and wider than the top-row gauges.
    constexpr int32_t kVacCx = 240, kVacCy = 211, kVacR = 82;
    gaugewidgets::drawArcGauge(tft_, vacuumArc_, kVacCx, kVacCy, kVacR, gaugeValue, gaugeMax, gaugeMax, gaugeMax,
                                theme_.background, theme_);
    tft_.setTextDatum(MC_DATUM);
    snprintf(buf, sizeof(buf), "%.1f", gaugeValue);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    beginLargeText(3);
    // A 2-digit reading plus the decimal point needs more room under Font 7 (32px/digit)
    // than the default font, same reasoning as the RPM field fix.
    int32_t vacFieldWidth = theme_.useSevenSegmentFont ? 112 : 96;
    gaugewidgets::drawFieldText(tft_, haveMap ? buf : "--", kVacCx, kVacCy - 14, vacFieldWidth, theme_.background);
    endLargeText();
    tft_.setTextSize(1);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    gaugewidgets::drawFieldText(tft_, gaugeLabel, kVacCx, kVacCy + 24, 90, theme_.background);
    gaugewidgets::drawFieldText(tft_, gaugeUnit, kVacCx, kVacCy + 36, 90, theme_.background);

    snprintf(buf, sizeof(buf), "%.2f V", snapshot.o2B1S1V.value);
    gaugewidgets::drawValueBox(tft_, 10, 253, 140, labels::kLabelO2B1S1, buf, snapshot.o2B1S1V.valid, theme_);

    snprintf(buf, sizeof(buf), "%.2f V", snapshot.o2B2S1V.value);
    gaugewidgets::drawValueBox(tft_, 330, 253, 140, labels::kLabelO2B2S1, buf, snapshot.o2B2S1V.valid, theme_);
}

// ---------------------------------------------------------------- Page 4: Performance & Telemetry --

void ClusterPages::drawPage4Static() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);
    tft_.fillRoundRect(10, 50, 220, 65, 6, theme_.panel);
    tft_.fillRoundRect(250, 50, 220, 65, 6, theme_.panel);
    tft_.fillRoundRect(140, 125, 200, 55, 6, theme_.panel);
    tft_.drawRoundRect(10, 190, 460, 110, 6, theme_.bezel);
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelIntakeAirflow60s, 16, 196);
}

void ClusterPages::drawPage4Dynamic(const TelemetrySnapshot& snapshot, uint32_t nowMs) {
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    char buf[20];

    bool haveMaf = snapshot.mafGps.valid;
    bool haveRpm = snapshot.rpm.valid && snapshot.rpm.value >= 500.0F;
    float hp = haveMaf ? snapshot.mafGps.value * 0.8F : 0.0F;
    float torque = (haveMaf && haveRpm) ? (hp * 5252.0F / snapshot.rpm.value) : 0.0F;

    snprintf(buf, sizeof(buf), "%.0f HP", hp);
    gaugewidgets::drawValueBox(tft_, 10, 58, 220, labels::kLabelEstHorsepower, buf, haveMaf, theme_);

    snprintf(buf, sizeof(buf), "%.0f lb-ft", torque);
    gaugewidgets::drawValueBox(tft_, 250, 58, 220, labels::kLabelEstTorque, buf, haveMaf && haveRpm, theme_);

    tft_.setTextDatum(TC_DATUM);
    tft_.setTextColor(theme_.textSecondary, theme_.panel);
    tft_.setTextSize(1);
    const char* timerLabel = metric ? labels::kLabelZeroToHundredKph : labels::kLabelZeroToSixty;
    tft_.drawString(timerLabel, 240, 132);
    tft_.setTextSize(2);
    if (runtimeState_.zeroToSixtyRunning) {
        float elapsed = (nowMs - runtimeState_.zeroToSixtyStartMs) / 1000.0F;
        snprintf(buf, sizeof(buf), "%.2f s", elapsed);
        tft_.setTextColor(theme_.primaryGaugeArc, theme_.panel);
    } else if (runtimeState_.zeroToSixtyResultSec >= 0.0F) {
        snprintf(buf, sizeof(buf), "%.2f s", runtimeState_.zeroToSixtyResultSec);
        tft_.setTextColor(theme_.textPrimary, theme_.panel);
    } else {
        strcpy(buf, "--");
        tft_.setTextColor(theme_.textSecondary, theme_.panel);
    }
    gaugewidgets::drawFieldText(tft_, buf, 240, 155, 190, theme_.panel);

    constexpr int32_t kGraphX = 12, kGraphY = 212, kGraphW = 456, kGraphH = 84;
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
            int32_t xA = kGraphX + (kGraphW * i) / (ClusterPageRuntimeState::kMafHistorySize - 1);
            int32_t xB = kGraphX + (kGraphW * (i + 1)) / (ClusterPageRuntimeState::kMafHistorySize - 1);
            int32_t yA = kGraphY + kGraphH - static_cast<int32_t>((runtimeState_.mafHistory[i] / maxSeen) * kGraphH);
            int32_t yB =
                kGraphY + kGraphH - static_cast<int32_t>((runtimeState_.mafHistory[i + 1] / maxSeen) * kGraphH);
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
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelActiveTheme, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Units row
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelUnits, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow1Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Gauge ticks toggle row
    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelGaugeTicks, 12, layout::kConfigRow2Y + layout::kConfigRowHeight / 2);

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

    snprintf(buf, sizeof(buf), "%s", settings.showGaugeTicks ? labels::kLabelTicksOn : labels::kLabelTicksOff);
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

    for (int i = 0; i <= 1; ++i) {
        int32_t y = layout::kHeaderHeight + i * layout::kConfigRowHeight;
        tft_.drawFastHLine(0, y, layout::kScreenWidth, theme_.bezel);
    }

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelBoostBaroBaseline, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);

    tft_.drawRoundRect(layout::kConfigMinusX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigMinusW, layout::kConfigButtonH, 4, theme_.bezel);
    tft_.drawRoundRect(layout::kConfigPlusX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigPlusW, layout::kConfigButtonH, 4, theme_.bezel);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    tft_.setTextSize(2);
    tft_.drawString(labels::kStepperMinus, layout::kConfigMinusX + layout::kConfigMinusW / 2,
                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kStepperPlus, layout::kConfigPlusX + layout::kConfigPlusW / 2,
                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2);

    // Force drawConfigUserVarsDynamic() to repaint every region the next time
    // it runs, since the static redraw above just wiped them all.
    runtimeState_.cfgBaroBaselineDrawn[0] = '\0';
}

void ClusterPages::drawConfigUserVarsDynamic(uint32_t nowMs) {
    (void)nowMs;
    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;
    char buf[24];

    if (metric) {
        float kpa = units::kpaFromPsi(settings.baroBaselinePsi);
        snprintf(buf, sizeof(buf), "%.1f %s", kpa, units::pressureUnitLabel(metric));
    } else {
        snprintf(buf, sizeof(buf), "%.1f %s", settings.baroBaselinePsi, units::pressureUnitLabel(metric));
    }
    if (strcmp(buf, runtimeState_.cfgBaroBaselineDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigValueX + layout::kConfigValueW / 2,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, layout::kConfigValueW,
                                     theme_.background);
        strncpy(runtimeState_.cfgBaroBaselineDrawn, buf, sizeof(runtimeState_.cfgBaroBaselineDrawn) - 1);
    }
}

// ---------------------------------------------------------------- Config: Gauges --

void ClusterPages::drawConfigGaugesStatic() {
    tft_.fillRect(0, layout::kHeaderHeight, layout::kScreenWidth, layout::kScreenHeight - layout::kHeaderHeight,
                  theme_.background);

    for (int i = 0; i <= 4; ++i) {
        int32_t y = layout::kHeaderHeight + i * layout::kConfigRowHeight;
        tft_.drawFastHLine(0, y, layout::kScreenWidth, theme_.bezel);
    }

    const AppSettings& settings = configStore_.settings();
    bool metric = settings.useMetricUnits;

    tft_.setTextDatum(ML_DATUM);
    tft_.setTextColor(theme_.textPrimary, theme_.background);
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelShiftLightRpm, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelRedlineRpm, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelMaxRpm, 12, layout::kConfigRow2Y + layout::kConfigRowHeight / 2);
    const char* maxSpeedLabel = metric ? labels::kLabelMaxSpeedKph : labels::kLabelMaxSpeedMph;
    tft_.drawString(maxSpeedLabel, 12, layout::kConfigRow3Y + layout::kConfigRowHeight / 2);

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

    // Force drawConfigGaugesDynamic() to repaint every region the next time
    // it runs, since the static redraw above just wiped them all.
    runtimeState_.cfgShiftLightRpmDrawn[0] = '\0';
    runtimeState_.cfgRedlineRpmDrawn[0] = '\0';
    runtimeState_.cfgMaxRpmDrawn[0] = '\0';
    runtimeState_.cfgMaxSpeedDrawn[0] = '\0';
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
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelLogInterval, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);

    tft_.drawRoundRect(layout::kConfigCycleX, layout::kConfigRow0Y + layout::kConfigButtonInsetY,
                        layout::kConfigCycleW, layout::kConfigButtonH, 4, theme_.bezel);

    // Log Units row (row 1), with the warning drawn directly below it
    tft_.drawString(labels::kLabelLogUnits, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);
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

    snprintf(buf, sizeof(buf), "%lu ms", static_cast<unsigned long>(settings.logIntervalMs));
    if (strcmp(buf, runtimeState_.cfgLogIntervalDrawn) != 0) {
        tft_.setTextDatum(MC_DATUM);
        tft_.setTextColor(theme_.textPrimary, theme_.background);
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, buf, layout::kConfigCycleX + layout::kConfigCycleW / 2,
                                     layout::kConfigRow0Y + layout::kConfigRowHeight / 2, layout::kConfigCycleW - 8,
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
        snprintf(buf, sizeof(buf), "%lu LOGS, %.1f MB", static_cast<unsigned long>(summary.fileCount), totalMb);
        if (strcmp(buf, runtimeState_.cfgLogSummaryDrawn) != 0) {
            tft_.setTextDatum(ML_DATUM);
            tft_.setTextColor(theme_.textPrimary, theme_.background);
            tft_.setTextSize(1);
            gaugewidgets::drawFieldText(tft_, buf, 12, layout::kLogsSummaryRowY + layout::kConfigRowHeight / 2,
                                         layout::kConfigDeleteX - 20, theme_.background);
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
    tft_.setTextSize(1);
    tft_.drawString(labels::kLabelObdAdapterName, 12, layout::kConfigRow0Y + layout::kConfigRowHeight / 2);
    tft_.drawString(labels::kLabelObdAdapterPin, 12, layout::kConfigRow1Y + layout::kConfigRowHeight / 2);

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
        tft_.setTextSize(2);
        gaugewidgets::drawFieldText(tft_, milOn ? labels::kStatusMilActive : labels::kStatusMilInactive,
                                     layout::kScreenWidth / 2, 46, 300, theme_.background);
        runtimeState_.dtcMilOnDrawn = static_cast<int8_t>(milOn);
    }

    DtcList dtcList;
    obdClient_.getDtcList(dtcList);
    bool haveResult = obdClient_.hasDtcResult();

    bool dtcListChanged = runtimeState_.dtcHaveResultDrawn != static_cast<int8_t>(haveResult) ||
                           (haveResult && !dtcListsEqual(dtcList, runtimeState_.dtcListDrawn));
    if (dtcListChanged) {
        tft_.setTextSize(2);
        // Clear the whole box every time regardless of how many lines will actually be
        // shown, so a shrinking list (fewer codes, or a fresh read) never leaves a stale
        // line from a taller previous draw.
        for (uint8_t line = 0; line < layout::kDtcListVisibleLines; ++line) {
            int32_t y = layout::kDtcListY + line * layout::kDtcListLineHeight;
            tft_.fillRect(30, y, 420, layout::kDtcListLineHeight - 2, theme_.background);
        }

        // Center the actual content block vertically within the box's 7-line span, and
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
