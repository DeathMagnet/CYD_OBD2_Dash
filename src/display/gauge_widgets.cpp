#include "display/gauge_widgets.h"

namespace gaugewidgets {

namespace {
constexpr float kGaugeStartAngle = 30.0F;
constexpr float kGaugeEndAngle = 330.0F;
constexpr int32_t kArcThicknessPx = 12;
} // namespace

void drawArcGauge(TFT_eSPI& tft, ArcGaugeState& state, int32_t centerX, int32_t centerY, int32_t radius,
                   float value, float maxValue, float cautionStart, float dangerStart, uint16_t bgColor,
                   const ThemeColors& theme) {
    float clampedValue = value < 0.0F ? 0.0F : (value > maxValue ? maxValue : value);
    float valueFraction = (maxValue > 0.0F) ? (clampedValue / maxValue) : 0.0F;
    float valueAngle = kGaugeStartAngle + valueFraction * (kGaugeEndAngle - kGaugeStartAngle);
    int32_t innerRadius = radius - kArcThicknessPx;

    // dangerStart may exceed maxValue if a user-configured redline (Page 5)
    // sits above the gauge's fixed max; clamp so the danger zone never runs
    // past the arc. cautionStart (Shift Light RPM) is likewise clamped so it
    // can never sit past dangerStart, which would invert the caution zone.
    float clampedDangerStart = dangerStart > maxValue ? maxValue : dangerStart;
    float clampedCautionStart = cautionStart > clampedDangerStart ? clampedDangerStart : cautionStart;
    bool hasCaution = clampedCautionStart < clampedDangerStart;
    bool hasDanger = clampedDangerStart < maxValue;

    // The caution/danger zones paint over the value arc, so inside them the fill
    // state is invisible and only the sweep below zoneStartAngle ever needs repainting.
    float zoneStartValue = hasCaution ? clampedCautionStart : clampedDangerStart;
    float zoneStartAngle = kGaugeStartAngle + (zoneStartValue / maxValue) * (kGaugeEndAngle - kGaugeStartAngle);
    float dangerStartAngle = kGaugeStartAngle + (clampedDangerStart / maxValue) * (kGaugeEndAngle - kGaugeStartAngle);

    int32_t limitDeg = static_cast<int32_t>(zoneStartAngle);
    int32_t newDeg = static_cast<int32_t>(valueAngle);

    if (!state.needsFullRedraw && maxValue == state.lastMaxValue) {
        int32_t lastDeg = static_cast<int32_t>(state.lastValueAngle);
        if (newDeg == lastDeg) {
            return;
        }
        int32_t valueTo = newDeg > limitDeg ? limitDeg : newDeg;

        if (newDeg < lastDeg) {
            // Erased as one continuous run out to the first zone (rather than just the
            // vacated slice) for the same anti-aliasing reason as the fill below.
            if (limitDeg > valueTo) {
                tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                                   static_cast<uint32_t>(valueTo), static_cast<uint32_t>(limitDeg),
                                   theme.secondaryGaugeArc, bgColor, false);
            }
            // That run's end feathers into the first zone's edge; repaint it.
            if (hasCaution) {
                tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                                   static_cast<uint32_t>(zoneStartAngle), static_cast<uint32_t>(dangerStartAngle),
                                   theme.cautionArc, bgColor, false);
            } else if (hasDanger) {
                tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                                   static_cast<uint32_t>(zoneStartAngle), static_cast<uint32_t>(kGaugeEndAngle),
                                   theme.dangerArc, bgColor, false);
            }
        }

        // Always redrawn from the start of the sweep: drawSmoothArc anti-aliases each
        // call's ends against bgColor, so stitching short segments leaves dark seams.
        // Repainting the same pixels in the same color costs one call and cannot flicker.
        if (valueTo > static_cast<int32_t>(kGaugeStartAngle)) {
            tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                               static_cast<uint32_t>(kGaugeStartAngle), static_cast<uint32_t>(valueTo),
                               theme.primaryGaugeArc, bgColor, false);
        }

        state.lastValueAngle = valueAngle;
        return;
    }

    tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                       static_cast<uint32_t>(kGaugeStartAngle), static_cast<uint32_t>(kGaugeEndAngle),
                       theme.secondaryGaugeArc, bgColor, false);

    if (valueFraction > 0.001F) {
        int32_t valueTo = newDeg > limitDeg ? limitDeg : newDeg;
        if (valueTo > static_cast<int32_t>(kGaugeStartAngle)) {
            tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                               static_cast<uint32_t>(kGaugeStartAngle), static_cast<uint32_t>(valueTo),
                               theme.primaryGaugeArc, bgColor, false);
        }
    }

    if (hasCaution) {
        tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                           static_cast<uint32_t>(zoneStartAngle), static_cast<uint32_t>(dangerStartAngle),
                           theme.cautionArc, bgColor, false);
    }

    if (hasDanger) {
        // Hold the danger color out to the end of the arc rather than
        // dropping back to the unlit track past the configured redline.
        tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                           static_cast<uint32_t>(dangerStartAngle), static_cast<uint32_t>(kGaugeEndAngle),
                           theme.dangerArc, bgColor, false);
    }

    state.lastValueAngle = valueAngle;
    state.lastMaxValue = maxValue;
    state.needsFullRedraw = false;
}

void drawBarGauge(TFT_eSPI& tft, BarGaugeState& state, int32_t x, int32_t y, int32_t width, int32_t height,
                   float percent, uint16_t fillColor, const ThemeColors& theme) {
    float clamped = percent < 0.0F ? 0.0F : (percent > 100.0F ? 100.0F : percent);
    int32_t innerWidth = width - 4;
    int32_t fillWidth = static_cast<int32_t>((clamped / 100.0F) * innerWidth);
    int32_t innerY = y + 2, innerH = height - 4;

    if (!state.needsFullRedraw) {
        if (fillWidth == state.lastFillWidth) {
            return;
        }
        if (fillWidth > state.lastFillWidth) {
            // Grew: paint only the newly covered sliver in fill color.
            tft.fillRect(x + 2 + state.lastFillWidth, innerY, fillWidth - state.lastFillWidth, innerH, fillColor);
        } else {
            // Shrank: paint only the vacated sliver back to background.
            tft.fillRect(x + 2 + fillWidth, innerY, state.lastFillWidth - fillWidth, innerH, theme.panel);
        }
        state.lastFillWidth = fillWidth;
        return;
    }

    tft.drawRect(x, y, width, height, theme.bezel);
    tft.fillRect(x + 2, innerY, innerWidth, innerH, theme.panel);
    if (fillWidth > 0) {
        tft.fillRect(x + 2, innerY, fillWidth, innerH, fillColor);
    }
    state.lastFillWidth = fillWidth;
    state.needsFullRedraw = false;
}

void drawValueBox(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width,
                   const char* label, const char* formattedValue, bool valid, const ThemeColors& theme) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(theme.textSecondary, theme.panel);
    tft.setTextSize(1);
    tft.drawString(label, x + width / 2, y);

    tft.setTextColor(valid ? theme.textPrimary : theme.textSecondary, theme.panel);
    tft.setTextSize(2);
    drawFieldText(tft, valid ? formattedValue : "--", x + width / 2, y + 14, width - 4, theme.panel);
}

void drawFieldText(TFT_eSPI& tft, const char* text, int32_t x, int32_t y,
                    int32_t fieldWidth, uint16_t background) {
    int32_t textWidth = tft.textWidth(text);
    int32_t textHeight = tft.fontHeight();
    uint8_t datum = tft.getTextDatum();

    int32_t textLeft = x;
    int32_t fieldLeft = x;
    switch (datum % 3) {
        case 1: // Centered
            textLeft = x - textWidth / 2;
            fieldLeft = x - fieldWidth / 2;
            break;
        case 2: // Right aligned
            textLeft = x - textWidth;
            fieldLeft = x - fieldWidth;
            break;
        default: break;
    }

    int32_t top = y;
    if (datum <= 8) {
        switch (datum / 3) {
            case 1: top = y - textHeight / 2; break;
            case 2: top = y - textHeight; break;
            default: break;
        }
    }

    int32_t leftGap = textLeft - fieldLeft;
    if (leftGap > 0) {
        tft.fillRect(fieldLeft, top, leftGap, textHeight, background);
    }
    int32_t textRight = textLeft + textWidth;
    int32_t rightGap = (fieldLeft + fieldWidth) - textRight;
    if (rightGap > 0) {
        tft.fillRect(textRight, top, rightGap, textHeight, background);
    }

    tft.drawString(text, x, y);
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
