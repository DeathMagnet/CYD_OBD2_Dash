#include "display/gauge_widgets.h"
#include <math.h>

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

void drawGaugeTicks(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius, float value, float maxValue,
                     float minorInterval, float majorInterval, float excludeFromValue, const ThemeColors& theme) {
    if (minorInterval <= 0.0F || maxValue <= 0.0F) {
        return;
    }
    constexpr float kDegToRad = 3.14159265F / 180.0F;
    constexpr int32_t kMinorTickLenPx = 4;
    constexpr int32_t kMajorTickLenPx = 8;
    int32_t innerRadius = radius - kArcThicknessPx;
    int majorStep = static_cast<int>(majorInterval / minorInterval + 0.5F);
    if (majorStep < 1) {
        majorStep = 1;
    }

    int tickCount = static_cast<int>(maxValue / minorInterval);
    for (int i = 1; i < tickCount; ++i) {
        float tickValue = static_cast<float>(i) * minorInterval;
        if (tickValue >= excludeFromValue) {
            continue;
        }
        float angle = kGaugeStartAngle + (tickValue / maxValue) * (kGaugeEndAngle - kGaugeStartAngle);
        float angleRad = angle * kDegToRad;
        float dirX = -sinf(angleRad);
        float dirY = cosf(angleRad);

        bool isMajor = (i % majorStep) == 0;
        int32_t tickLen = isMajor ? kMajorTickLenPx : kMinorTickLenPx;
        int32_t xOuter0 = centerX + static_cast<int32_t>(dirX * radius);
        int32_t yOuter0 = centerY + static_cast<int32_t>(dirY * radius);
        int32_t xOuter1 = centerX + static_cast<int32_t>(dirX * (radius + tickLen));
        int32_t yOuter1 = centerY + static_cast<int32_t>(dirY * (radius + tickLen));
        int32_t xInner0 = centerX + static_cast<int32_t>(dirX * (innerRadius - tickLen));
        int32_t yInner0 = centerY + static_cast<int32_t>(dirY * (innerRadius - tickLen));
        int32_t xInner1 = centerX + static_cast<int32_t>(dirX * innerRadius);
        int32_t yInner1 = centerY + static_cast<int32_t>(dirY * innerRadius);

        uint16_t tickColor = (value >= tickValue) ? theme.primaryGaugeArc : theme.tickInactiveColor;
        if (theme.showOuterTicks) {
            tft.drawLine(xOuter0, yOuter0, xOuter1, yOuter1, tickColor);  // outside: radius to radius+tickLen
        }
        tft.drawLine(xInner0, yInner0, xInner1, yInner1, tickColor);   // inside: innerRadius-tickLen to innerRadius
    }
}

void drawGaugeBezel(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius, const ThemeColors& theme) {
    if (!theme.showGaugeBezel) {
        return;
    }
    constexpr int32_t kBezelGap = 2;       // px between the arc's outer edge and the ring
    constexpr int32_t kBezelThickness = 4; // ring width in px
    for (int32_t i = 0; i < kBezelThickness; ++i) {
        tft.drawCircle(centerX, centerY, radius + kBezelGap + i, theme.bezel);
    }
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
                   const char* label, const char* formattedValue, bool valid, const ThemeColors& theme,
                   uint8_t labelTextSize, uint8_t valueTextSize) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(theme.textSecondary, theme.panel);
    tft.setTextSize(labelTextSize);
    tft.drawString(label, x + width / 2, y);

    // Default label height (size 1) is ~8px; scale the gap with labelTextSize so a
    // bumped caption doesn't collide with the value below it. 8*1+6=14, matching the
    // original fixed offset exactly when labelTextSize is left at its default.
    int32_t valueY = y + 8 * labelTextSize + 6;
    tft.setTextColor(valid ? theme.textPrimary : theme.textSecondary, theme.panel);
    tft.setTextSize(valueTextSize);
    drawFieldText(tft, valid ? formattedValue : "--", x + width / 2, valueY, width - 4, theme.panel);
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
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);

    // Clear the indicator's full glyph bounding box (plus the 1px bold offset
    // and a small margin) on every redraw to prevent stale pixels when the
    // MIL transitions from on to off. A fixed-radius circle here previously
    // left slivers uncleared since it was narrower than the "!!!" text.
    int32_t textW = tft.textWidth("!!!") + 1;
    int32_t textH = tft.fontHeight();
    constexpr int32_t kClearPad = 3;
    tft.fillRect(centerX - textW / 2 - kClearPad, centerY - textH / 2 - kClearPad,
                 textW + kClearPad * 2, textH + kClearPad * 2, theme.panel);

    if (!milOn) {
        return; // Indicator only shown when MIL is active
    }

    // Draw three bold exclamation points in warning red
    tft.setTextColor(theme.warningActive, theme.panel);

    // Simulate bold by drawing twice with a 1px horizontal offset
    tft.drawString("!!!", centerX - 1, centerY);
    tft.drawString("!!!", centerX, centerY);
}

void drawModeToggleButton(TFT_eSPI& tft, int32_t centerX, int32_t centerY, bool showGear, const ThemeColors& theme) {
    constexpr float kDegToRad = 3.14159265F / 180.0F;

    if (showGear) {
        // Cog/settings icon: a solid body ring with short radial teeth,
        // sized to sit just under the steering wheel's footprint below.
        constexpr int32_t kBodyRadius = 6;
        constexpr int32_t kToothLength = 4;
        constexpr float kToothWidth = 3.0F;
        constexpr int32_t kHubRadius = 2;
        constexpr int kToothCount = 8;

        tft.fillCircle(centerX, centerY, kBodyRadius, theme.textSecondary);
        for (int i = 0; i < kToothCount; ++i) {
            float angle = (360.0F / kToothCount) * static_cast<float>(i) * kDegToRad;
            float cosA = cosf(angle);
            float sinA = sinf(angle);
            float x0 = centerX + cosA * kBodyRadius;
            float y0 = centerY + sinA * kBodyRadius;
            float x1 = centerX + cosA * (kBodyRadius + kToothLength);
            float y1 = centerY + sinA * (kBodyRadius + kToothLength);
            tft.drawWideLine(x0, y0, x1, y1, kToothWidth, theme.textSecondary);
        }
        tft.fillCircle(centerX, centerY, kHubRadius, theme.panel);
    } else {
        constexpr int32_t kRimRadius = 12;
        constexpr int32_t kHubRadius = 3;
        constexpr float kSpokeAngles[3] = {90.0F, 210.0F, 330.0F};

        tft.drawCircle(centerX, centerY, kRimRadius, theme.textSecondary);
        tft.drawCircle(centerX, centerY, kRimRadius - 1, theme.textSecondary);
        for (float angleDeg : kSpokeAngles) {
            float angle = angleDeg * kDegToRad;
            int32_t hx = centerX + static_cast<int32_t>(cosf(angle) * kHubRadius);
            int32_t hy = centerY + static_cast<int32_t>(sinf(angle) * kHubRadius);
            int32_t rx = centerX + static_cast<int32_t>(cosf(angle) * kRimRadius);
            int32_t ry = centerY + static_cast<int32_t>(sinf(angle) * kRimRadius);
            tft.drawLine(hx, hy, rx, ry, theme.textSecondary);
        }
        tft.fillCircle(centerX, centerY, kHubRadius, theme.textSecondary);
    }
}

} // namespace gaugewidgets
