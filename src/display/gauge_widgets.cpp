#include "display/gauge_widgets.h"

namespace gaugewidgets {

namespace {
constexpr float kGaugeStartAngle = 30.0F;
constexpr float kGaugeEndAngle = 330.0F;
constexpr int32_t kArcThicknessPx = 12;
constexpr int kRedlineGradientSegments = 12;

uint16_t lerpColor565(uint16_t colorA, uint16_t colorB, float t) {
    uint8_t r1 = (colorA >> 11) & 0x1F, g1 = (colorA >> 5) & 0x3F, b1 = colorA & 0x1F;
    uint8_t r2 = (colorB >> 11) & 0x1F, g2 = (colorB >> 5) & 0x3F, b2 = colorB & 0x1F;
    uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * t);
    uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * t);
    uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * t);
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
} // namespace

void drawArcGauge(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius,
                   float value, float maxValue, float redlineStart, float redlineEnd, uint16_t bgColor,
                   const ThemeColors& theme) {
    float clampedValue = value < 0.0F ? 0.0F : (value > maxValue ? maxValue : value);
    float valueFraction = (maxValue > 0.0F) ? (clampedValue / maxValue) : 0.0F;
    float valueAngle = kGaugeStartAngle + valueFraction * (kGaugeEndAngle - kGaugeStartAngle);
    int32_t innerRadius = radius - kArcThicknessPx;

    tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                       static_cast<uint32_t>(kGaugeStartAngle), static_cast<uint32_t>(kGaugeEndAngle),
                       theme.secondaryGaugeArc, bgColor, false);

    if (valueFraction > 0.001F) {
        tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                           static_cast<uint32_t>(kGaugeStartAngle), static_cast<uint32_t>(valueAngle),
                           theme.primaryGaugeArc, bgColor, false);
    }

    // redlineEnd may be below redlineStart if a user-configured redline
    // (Page 5) is set lower than the fixed OEM curve start; clamp so the
    // zone never inverts and always ends by maxValue.
    float clampedRedlineEnd = redlineEnd > maxValue ? maxValue : redlineEnd;
    if (redlineStart < clampedRedlineEnd) {
        // Drawn as short interpolated-color segments rather than one solid
        // arc so the redline reads as a true gradient (per the UI cluster
        // guide's "Redline Arc" spec), since drawSmoothArc only takes one
        // flat foreground color per call.
        float redlineStartFraction = redlineStart / maxValue;
        float redlineEndFraction = clampedRedlineEnd / maxValue;
        float redlineStartAngle = kGaugeStartAngle + redlineStartFraction * (kGaugeEndAngle - kGaugeStartAngle);
        float redlineEndAngle = kGaugeStartAngle + redlineEndFraction * (kGaugeEndAngle - kGaugeStartAngle);
        float totalSweep = redlineEndAngle - redlineStartAngle;

        for (int segment = 0; segment < kRedlineGradientSegments; ++segment) {
            float segStart = redlineStartAngle + (totalSweep * segment) / kRedlineGradientSegments;
            float segEnd = redlineStartAngle + (totalSweep * (segment + 1)) / kRedlineGradientSegments;
            float t = static_cast<float>(segment) / (kRedlineGradientSegments - 1);
            uint16_t segColor = lerpColor565(theme.redlineGradientStart, theme.redlineGradientEnd, t);
            tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                               static_cast<uint32_t>(segStart), static_cast<uint32_t>(segEnd),
                               segColor, bgColor, false);
        }
    }
}

void drawBarGauge(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, int32_t height,
                   float percent, uint16_t fillColor, const ThemeColors& theme) {
    float clamped = percent < 0.0F ? 0.0F : (percent > 100.0F ? 100.0F : percent);
    int32_t innerWidth = width - 4;
    int32_t fillWidth = static_cast<int32_t>((clamped / 100.0F) * innerWidth);

    tft.drawRect(x, y, width, height, theme.bezel);
    tft.fillRect(x + 2, y + 2, innerWidth, height - 4, theme.panel);
    if (fillWidth > 0) {
        tft.fillRect(x + 2, y + 2, fillWidth, height - 4, fillColor);
    }
}

void drawValueBox(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width,
                   const char* label, const char* formattedValue, bool valid, const ThemeColors& theme) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(theme.textSecondary, theme.panel);
    tft.setTextSize(1);
    tft.drawString(label, x + width / 2, y);

    tft.setTextColor(valid ? theme.textPrimary : theme.textSecondary, theme.panel);
    tft.setTextSize(2);
    tft.drawString(valid ? formattedValue : "--", x + width / 2, y + 14);
}

void drawStatusBadge(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, int32_t height,
                      const char* text, uint16_t badgeColor, uint16_t textColor) {
    tft.fillRoundRect(x, y, width, height, 4, badgeColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(textColor, badgeColor);
    tft.setTextSize(1);
    tft.drawString(text, x + width / 2, y + height / 2);
}

void drawMilIndicator(TFT_eSPI& tft, int32_t centerX, int32_t centerY, bool milOn, const ThemeColors& theme) {
    uint16_t ringColor = milOn ? theme.warningActive : theme.textSecondary;
    tft.fillCircle(centerX, centerY, 10, milOn ? ringColor : theme.panel);
    tft.drawCircle(centerX, centerY, 10, ringColor);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(milOn ? theme.background : ringColor, milOn ? ringColor : theme.panel);
    tft.setTextSize(1);
    tft.drawString("CEL", centerX, centerY);
}

} // namespace gaugewidgets
