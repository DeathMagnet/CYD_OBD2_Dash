#include "display/gauge_widgets.h"
#include <math.h>

namespace gaugewidgets {

namespace {
constexpr float kGaugeStartAngle = 30.0F;
constexpr float kGaugeEndAngle = 330.0F;
constexpr int32_t kArcThicknessPx = 12;

// Segmented-arc LED color for one wedge: danger/caution zones always win (they're a
// fixed-position overlay, not "unlocked" by the value reaching them), otherwise lit
// vs. unlit by whether this segment's index falls under the current value fill.
uint16_t segmentedArcColor(int32_t index, int32_t litSegments, bool hasCaution, int32_t cautionSegmentIndex,
                            bool hasDanger, int32_t dangerSegmentIndex, const ThemeColors& theme) {
    if (hasDanger && index >= dangerSegmentIndex) return theme.dangerArc;
    if (hasCaution && index >= cautionSegmentIndex) return theme.cautionArc;
    return (index < litSegments) ? theme.primaryGaugeArc : theme.secondaryGaugeArc;
}
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

    if (theme.useSegmentedArcs) {
        constexpr float kSegmentAngleWidthDeg = 6.0F; // LED segment angular width, degrees -- tune here.
        constexpr float kSegmentGapDeg = 2.0F;        // gap between segments, degrees -- tune here.
        constexpr float kSegmentPitchDeg = kSegmentAngleWidthDeg + kSegmentGapDeg;
        constexpr float kSweepDeg = kGaugeEndAngle - kGaugeStartAngle;

        int32_t numSegments = static_cast<int32_t>((kSweepDeg + kSegmentGapDeg) / kSegmentPitchDeg);
        if (numSegments < 1) numSegments = 1;
        int32_t litSegments = static_cast<int32_t>(valueFraction * static_cast<float>(numSegments) + 0.5F);
        if (litSegments > numSegments) litSegments = numSegments;

        int32_t cautionSegmentIndex = hasCaution
            ? static_cast<int32_t>((clampedCautionStart / maxValue) * static_cast<float>(numSegments) + 0.5F)
            : numSegments;
        int32_t dangerSegmentIndex = hasDanger
            ? static_cast<int32_t>((clampedDangerStart / maxValue) * static_cast<float>(numSegments) + 0.5F)
            : numSegments;

        int32_t from = 0;
        int32_t to = numSegments;
        bool doDelta = !state.needsFullRedraw && maxValue == state.lastMaxValue;
        if (doDelta) {
            if (litSegments == state.lastLitSegments) {
                return;
            }
            from = litSegments < state.lastLitSegments ? litSegments : state.lastLitSegments;
            to = litSegments > state.lastLitSegments ? litSegments : state.lastLitSegments;
        }

        for (int32_t i = from; i < to; ++i) {
            float segStart = kGaugeStartAngle + static_cast<float>(i) * kSegmentPitchDeg;
            float segEnd = segStart + kSegmentAngleWidthDeg;
            tft.drawSmoothArc(centerX, centerY, radius, innerRadius,
                               static_cast<uint32_t>(segStart), static_cast<uint32_t>(segEnd),
                               segmentedArcColor(i, litSegments, hasCaution, cautionSegmentIndex,
                                                  hasDanger, dangerSegmentIndex, theme),
                               bgColor, false);
        }

        state.lastLitSegments = litSegments;
        state.lastValueAngle = valueAngle;
        state.lastMaxValue = maxValue;
        state.needsFullRedraw = false;
        return;
    }

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
                     float minorInterval, float majorInterval, float excludeFromValue, TickMode tickMode,
                     const ThemeColors& theme) {
    if (minorInterval <= 0.0F || maxValue <= 0.0F || tickMode == TickMode::Off) {
        return;
    }
    bool drawInner = tickMode == TickMode::InsideOnly || tickMode == TickMode::InsideAndOutside;
    bool drawOuter = theme.showOuterTicks &&
                      (tickMode == TickMode::OutsideOnly || tickMode == TickMode::InsideAndOutside);
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
        if (drawOuter) {
            tft.drawLine(xOuter0, yOuter0, xOuter1, yOuter1, tickColor);  // outside: radius to radius+tickLen
        }
        if (drawInner) {
            tft.drawLine(xInner0, yInner0, xInner1, yInner1, tickColor);   // inside: innerRadius-tickLen to innerRadius
        }
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
    int32_t innerX = x + 2, innerY = y + 2, innerH = height - 4;

    if (theme.useSegmentedBars) {
        constexpr int32_t kSegmentWidthPx = 8; // LED segment width, px -- tune here.
        constexpr int32_t kSegmentGapPx = 3;   // gap between segments, px -- tune here.
        constexpr int32_t kSegmentPitchPx = kSegmentWidthPx + kSegmentGapPx;

        int32_t numSegments = (innerWidth + kSegmentGapPx) / kSegmentPitchPx;
        if (numSegments < 1) numSegments = 1;
        int32_t litSegments = static_cast<int32_t>(clamped / 100.0F * static_cast<float>(numSegments) + 0.5F);
        if (litSegments > numSegments) litSegments = numSegments;

        if (!state.needsFullRedraw) {
            if (litSegments == state.lastLitSegments) {
                return;
            }
            if (litSegments > state.lastLitSegments) {
                // Grew: light the newly lit segments.
                for (int32_t i = state.lastLitSegments; i < litSegments; ++i) {
                    tft.fillRect(innerX + i * kSegmentPitchPx, innerY, kSegmentWidthPx, innerH, fillColor);
                }
            } else {
                // Shrank: darken the newly unlit segments.
                for (int32_t i = litSegments; i < state.lastLitSegments; ++i) {
                    tft.fillRect(innerX + i * kSegmentPitchPx, innerY, kSegmentWidthPx, innerH, theme.panel);
                }
            }
            state.lastLitSegments = litSegments;
            return;
        }

        tft.drawRect(x, y, width, height, theme.bezel);
        tft.fillRect(innerX, innerY, innerWidth, innerH, theme.panel); // clears unlit segments + gaps in one call
        for (int32_t i = 0; i < litSegments; ++i) {
            tft.fillRect(innerX + i * kSegmentPitchPx, innerY, kSegmentWidthPx, innerH, fillColor);
        }
        state.lastLitSegments = litSegments;
        state.needsFullRedraw = false;
        return;
    }

    int32_t fillWidth = static_cast<int32_t>((clamped / 100.0F) * innerWidth);

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
                   uint8_t labelTextSize, uint8_t valueTextSize, int32_t labelToValueGap) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(theme.textSecondary, theme.panel);
    tft.setTextSize(labelTextSize);
    tft.drawString(label, x + width / 2, y);

    tft.setTextColor(valid ? theme.textPrimary : theme.textSecondary, theme.panel);
    applyValueFont(tft, theme, 2);
    drawFieldText(tft, valid ? formattedValue : "--", x + width / 2, y + labelToValueGap, width - 4, theme.panel);
    resetValueFont(tft);
}

void drawFixedUnit(TFT_eSPI& tft, const char* unit, int32_t rightX, int32_t y, uint8_t datum,
                    uint8_t textSize, const ThemeColors& theme, uint16_t background) {
    tft.setTextDatum(datum);
    tft.setTextFont(1);
    tft.setTextSize(textSize);
    tft.setTextColor(theme.textSecondary, background);
    tft.drawString(unit, rightX, y);
}

int32_t fixedUnitWidth(TFT_eSPI& tft, const char* unit, uint8_t textSize) {
    tft.setTextFont(1);
    tft.setTextSize(textSize);
    return tft.textWidth(unit);
}

int32_t reservedValueWidth(TFT_eSPI& tft, const ThemeColors& theme, uint8_t sizeTier, const char* representative) {
    applyValueFont(tft, theme, sizeTier);
    int32_t w = tft.textWidth(representative);
    resetValueFont(tft);
    return w;
}

int32_t configValueWidth(TFT_eSPI& tft, const char* representative) {
    applyConfigValueFont(tft);
    int32_t w = tft.textWidth(representative);
    resetValueFont(tft);
    return w;
}

ValueUnitGroup centerValueUnitGroup(int32_t centerX, int32_t valueWidth, int32_t unitWidth, int32_t gapPx) {
    int32_t groupWidth = valueWidth + gapPx + unitWidth;
    int32_t groupLeft = centerX - groupWidth / 2;
    return {groupLeft + valueWidth, groupLeft + groupWidth};
}

void drawValueBoxStatic(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, const char* label,
                         const char* unit, const char* representativeValue, const ThemeColors& theme,
                         uint8_t labelTextSize, uint8_t valueTextSize, int32_t labelToValueGap) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(theme.textSecondary, theme.panel);
    tft.setTextSize(labelTextSize);
    tft.drawString(label, x + width / 2, y);

    if (unit != nullptr && unit[0] != '\0') {
        constexpr int32_t kGapPx = 4;
        int32_t unitW = fixedUnitWidth(tft, unit, valueTextSize);
        int32_t valueW = reservedValueWidth(tft, theme, 2, representativeValue);
        ValueUnitGroup group = centerValueUnitGroup(x + width / 2, valueW, unitW, kGapPx);
        drawFixedUnit(tft, unit, group.unitRightX, y + labelToValueGap, TR_DATUM, valueTextSize, theme, theme.panel);
    }
}

void drawValueBoxValue(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, const char* formattedValue,
                        const char* unit, const char* representativeValue, bool valid, const ThemeColors& theme,
                        uint8_t valueTextSize, int32_t labelToValueGap) {
    bool hasUnit = unit != nullptr && unit[0] != '\0';
    int32_t rightX;
    int32_t fieldWidth;
    if (hasUnit) {
        constexpr int32_t kGapPx = 4;
        int32_t unitW = fixedUnitWidth(tft, unit, valueTextSize);
        int32_t valueW = reservedValueWidth(tft, theme, 2, representativeValue);
        ValueUnitGroup group = centerValueUnitGroup(x + width / 2, valueW, unitW, kGapPx);
        rightX = group.valueRightX;
        fieldWidth = valueW;
    } else {
        rightX = x + width / 2;
        fieldWidth = width - 4;
    }

    tft.setTextDatum(hasUnit ? TR_DATUM : TC_DATUM);
    tft.setTextColor(valid ? theme.textPrimary : theme.textSecondary, theme.panel);
    applyValueFont(tft, theme, 2);
    drawFieldText(tft, valid ? formattedValue : "--", rightX, y + labelToValueGap, fieldWidth, theme.panel);
    resetValueFont(tft);
}

void drawFieldText(TFT_eSPI& tft, const char* text, int32_t x, int32_t y,
                    int32_t fieldWidth, uint16_t background) {
    int32_t textHeight = tft.fontHeight();
    uint8_t datum = tft.getTextDatum();

    int32_t fieldLeft = x;
    switch (datum % 3) {
        case 1: fieldLeft = x - fieldWidth / 2; break; // Centered
        case 2: fieldLeft = x - fieldWidth; break;      // Right aligned
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

    // Always clear the full field before drawing, rather than only the gaps
    // left/right of the new text's own bounds: relying on drawString()'s own
    // opaque-background erase to fully cover its footprint, combined with a
    // differential side-only clear here, left ghosting on some fonts/values.
    tft.fillRect(fieldLeft, top, fieldWidth, textHeight, background);
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
