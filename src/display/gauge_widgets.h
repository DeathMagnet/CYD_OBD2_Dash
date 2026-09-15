#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>
#include "display/theme.h"

// Critically damped spring smoothing for animated gauge values (see
// docs/cyd-obd2-ui-cluster-guide.md, "Gauge Needle Interpolation"). Call
// update() once per render frame with the latest valid reading; currentValue
// eases toward target instead of jumping.
struct NeedlePhysics {
    float currentValue = 0.0F;
    float velocity = 0.0F;

    void update(float target, float dtSeconds, float stiffness = 180.0F, float damping = 22.0F) {
        if (dtSeconds <= 0.0F) {
            return;
        }
        // Explicit integration diverges once damping * step approaches 2, and the UI
        // refreshes at ~100ms, so advance in fixed sub-steps rather than one frame-sized one.
        constexpr float kMaxStepSeconds = 0.01F;
        constexpr int kMaxSteps = 64;
        int steps = static_cast<int>(dtSeconds / kMaxStepSeconds) + 1;
        if (steps > kMaxSteps) {
            steps = kMaxSteps;
        }
        float step = dtSeconds / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            float accel = (target - currentValue) * stiffness - velocity * damping;
            velocity += accel * step;
            currentValue += velocity * step;
        }
    }
};

// This project draws directly to the TFT rather than through TFT_eSprite:
// the ESP32-32E on this board has no PSRAM, and a full-frame RGB565 sprite
// (480x320x2 bytes ~= 300KB) does not fit in its 320KB SRAM alongside the
// Bluetooth stack and SD buffers. Widgets instead redraw only their own
// bounded region each frame and rely on TFT_eSPI's built-in background-color
// text redraw to avoid flicker.
namespace gaugewidgets {

// Lets an arc gauge repaint only the sweep that actually moved between frames.
// Call invalidate() whenever the area behind the gauge is cleared (page redraw).
struct ArcGaugeState {
    float lastValueAngle = 0.0F;
    float lastMaxValue = 0.0F;
    int32_t lastLitSegments = -1; // Segmented-style theme only; unused otherwise.
    bool needsFullRedraw = true;

    void invalidate() { needsFullRedraw = true; }
};

// Circular arc gauge, 0..maxValue swept from 30 degrees to 330 degrees
// (bottom-left to bottom-right, opening at 6 o'clock) per TFT_eSPI's
// drawSmoothArc angle convention. `bgColor` must match the surface color
// immediately behind the gauge so anti-aliased edges blend correctly. Solid
// caution color (theme.cautionArc) paints from cautionStart to dangerStart,
// then solid danger color (theme.dangerArc) holds from dangerStart out to
// the end of the arc; pass cautionStart >= dangerStart (e.g. both equal to
// maxValue) to disable both zones for gauges that don't have a warning
// concept (load, vacuum/boost, etc).
void drawArcGauge(TFT_eSPI& tft, ArcGaugeState& state, int32_t centerX, int32_t centerY, int32_t radius,
                   float value, float maxValue, float cautionStart, float dangerStart, uint16_t bgColor,
                   const ThemeColors& theme);

// Needle gauge pair for theme.useNeedleGauge (S197 - Analog): call both once
// per frame, with any dynamic content that sits inside the needle's sweep
// (the center numeral, drawGaugeTickLabels()) redrawn in between, so the
// needle ends up drawn on top of it:
//   eraseNeedle(...);          // erase the previous frame's needle
//   ... redraw numeral, drawGaugeTickLabels() ...
//   drawNeedle(...);           // draw this frame's needle on top
// eraseNeedle() is a no-op before the first drawNeedle() call (nothing drawn
// yet), and also a no-op whenever `value` would round to the same angle as
// the last draw - erasing a needle that hasn't visibly moved just makes it
// flicker for the gap until drawNeedle() redraws it a few lines below, with
// nothing to show for it. Uses the same ArcGaugeState as drawArcGauge(), but
// the two are never used on the same gauge (a theme either fills an arc or
// draws a needle).
void eraseNeedle(TFT_eSPI& tft, ArcGaugeState& state, int32_t centerX, int32_t centerY, int32_t radius, float value,
                  float maxValue, uint16_t bgColor);

// Draws the needle at `value`'s angle (see drawArcGauge for the shared
// value/maxValue/cautionStart/dangerStart/bgColor convention), plus the hub
// cap on top. On the first call after invalidate() or a maxValue change,
// first paints the static dial face (track ring + caution/danger redline
// zone) - the fill-arc themes' equivalent, minus the value-fill arc itself,
// which the needle replaces. Updates `state` for the next frame's
// eraseNeedle() call.
void drawNeedle(TFT_eSPI& tft, ArcGaugeState& state, int32_t centerX, int32_t centerY, int32_t radius, float value,
                 float maxValue, float cautionStart, float dangerStart, uint16_t bgColor, const ThemeColors& theme);

// Radial tick marks at fixed value intervals along an arc gauge's scale,
// straddling the arc's outer edge (tickLen px inside and outside). Ticks at
// multiples of majorInterval are drawn longer ("a little bigger"). A tick is
// painted in theme.primaryGaugeArc once value >= that tick's location, and
// theme.tickInactiveColor while value < that location. Ticks at or above
// excludeFromValue are skipped entirely (pass >= maxValue to disable exclusion,
// matching drawArcGauge's convention for gauges with no warning zone).
// tickMode selects which segment(s) get drawn (Off is a no-op); the outer
// segment is further gated by theme.showOuterTicks since some themes (both S197 variants)
// have no room in their bezel art for it regardless of tickMode.
void drawGaugeTicks(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius, float value, float maxValue,
                     float minorInterval, float majorInterval, float excludeFromValue, TickMode tickMode,
                     const ThemeColors& theme);

// Numeric labels at major-tick positions for a needle gauge (theme.useNeedleGauge),
// e.g. "1".."9" around an RPM dial or "20".."140" around a speedometer, always
// drawn in white regardless of theme. The labels' own values never change, but
// the needle's tip can sweep across their radius (see eraseNeedle()/
// drawNeedle()), and eraseNeedle()'s flat-color erase would otherwise punch a
// permanent notch in them - so call this every frame from drawDynamic()
// (sandwiched between eraseNeedle() and drawNeedle(), like the center
// numeral) rather than once from drawStatic().
// `labelScale` divides each tick's raw value before formatting with `format`
// (e.g. scale=1000.0F, format="%.0f" prints "1000" as "1"); pass 1.0F/"%.0f" to
// print the raw value unscaled. `excludeFromValue` matches drawGaugeTicks'
// parameter of the same name (e.g. skip labels inside a redline zone).
// `smallFont` switches from the default `FreeSansBold9pt7b` to the small
// built-in GLCD font, for gauges (Speed, with its finer tick graduation) that
// pack too many labels into the arc for the bold free font to stay legible.
void drawGaugeTickLabels(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius, float maxValue,
                          float majorInterval, float excludeFromValue, float labelScale, const char* format,
                          const ThemeColors& theme, uint16_t background, bool smallFont = false);

// Decorative ring drawn just outside a round arc gauge's outer edge (OEM
// chrome bezel look). No-op unless theme.showGaugeBezel is set, so callers
// can invoke it unconditionally. Static chrome — call once from the owning
// page's drawStatic(), with centerX/centerY/radius matching the paired
// drawArcGauge() call exactly.
void drawGaugeBezel(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius, const ThemeColors& theme);

// Lets a bar gauge repaint only the sliver of fill that changed between
// frames instead of clearing and redrawing the whole bar. Call invalidate()
// whenever the area behind the gauge is cleared (page redraw).
struct BarGaugeState {
    int32_t lastFillWidth = -1;
    int32_t lastLitSegments = -1; // Segmented-style theme only; unused otherwise.
    bool needsFullRedraw = true;

    void invalidate() { needsFullRedraw = true; }
};

// Flat horizontal bar (0-100%) with outline and fill, used for
// throttle/load/trim style readouts.
void drawBarGauge(TFT_eSPI& tft, BarGaugeState& state, int32_t x, int32_t y, int32_t width, int32_t height,
                   float percent, uint16_t fillColor, const ThemeColors& theme);

// Draws a unit/suffix caption once, in the default font (Font 1) at
// `textSize`, colored theme.textSecondary against `background`, aligned per
// `datum` (must be a right-aligned datum: TR_DATUM/MR_DATUM/BR_DATUM) so its
// right edge sits at a fixed `rightX` regardless of any paired value's
// width. Call ONLY from a page's drawStatic()/drawConfigXStatic() — never
// from drawDynamic() — so the unit's position and pixels are never touched
// by per-tick refreshes. Pair with fixedUnitWidth() from the matching
// drawDynamic() to size the value field that butts up against it (see
// drawFieldText()'s doc comment).
void drawFixedUnit(TFT_eSPI& tft, const char* unit, int32_t rightX, int32_t y, uint8_t datum,
                    uint8_t textSize, const ThemeColors& theme, uint16_t background);

// Returns the pixel width `unit` occupies when drawn by drawFixedUnit() with
// the same string/textSize, so a paired drawDynamic() can right-align its
// value to end exactly at the unit's fixed left edge (minus a small gap)
// without overlapping it. Recomputed each call rather than cached: cheap,
// and keeps drawDynamic() free of any new per-field state.
int32_t fixedUnitWidth(TFT_eSPI& tft, const char* unit, uint8_t textSize);

// Returns the pixel width `representative` occupies when rendered in the
// theme's "large value" font at `sizeTier` (see applyValueFont) — i.e. how
// much horizontal room a value field should reserve so a value+unit group's
// layout can be computed from constants alone rather than the live value
// (see centerValueUnitGroup()). Pass a string representing the widest value
// the field realistically shows (e.g. "199" for a 3-digit reading). Leaves
// the active font as Font 1 afterward (matches resetValueFont()). For fields
// that just use Font 1 directly (no applyValueFont call), reuse
// fixedUnitWidth() instead — same idea, same font.
int32_t reservedValueWidth(TFT_eSPI& tft, const ThemeColors& theme, uint8_t sizeTier, const char* representative);

// Same idea as reservedValueWidth(), for config-page numeric values that use
// applyConfigValueFont() rather than a theme's applyValueFont() tier. Pass a
// string representing the widest value the field realistically shows (e.g.
// "199.9"). Leaves the active font as Font 1 afterward (matches
// resetValueFont()).
int32_t configValueWidth(TFT_eSPI& tft, const char* representative);

// Centers a `valueWidth`-wide value slot + `gapPx` + a unit of `unitWidth` as
// one block on `centerX`, returning the right edge of each half (for right-
// aligned draws ending there). Neither returned edge depends on the value
// actually drawn this frame — only on the two widths passed in — so calling
// this identically from drawStatic() (to place the unit via drawFixedUnit())
// and drawDynamic() (to place the value via drawFieldText()) keeps the
// unit's position exactly fixed while the pair still reads as centered in
// its container, instead of hugging one edge.
struct ValueUnitGroup {
    int32_t valueRightX;
    int32_t unitRightX;
};
ValueUnitGroup centerValueUnitGroup(int32_t centerX, int32_t valueWidth, int32_t unitWidth, int32_t gapPx);

// A caption above a large numeric readout. Renders "--" in textSecondary
// when !valid rather than a fabricated zero (see coding standards).
// labelTextSize/valueTextSize default to every existing caller's current
// sizes (1 and 2); pass larger values to bump just this call site.
// labelToValueGap defaults to every existing caller's current spacing (14px
// below the label's y); pass a larger value to widen just this call site.
//
// When `unit` is non-empty, split the call across the owning page's Static
// and Dynamic passes instead: drawValueBoxStatic() draws the label and the
// fixed unit caption once, and drawValueBoxValue() (called every refresh
// tick) draws only the number, right-aligned against the unit's fixed left
// edge. Use plain drawValueBox() only for fields with no unit at all.
void drawValueBox(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width,
                   const char* label, const char* formattedValue, bool valid, const ThemeColors& theme,
                   uint8_t labelTextSize = 1, uint8_t valueTextSize = 2, int32_t labelToValueGap = 14);

// Static half of a unit-bearing value box: draws the label (same position as
// drawValueBox()) plus, when `unit` is non-empty, a fixed unit caption
// positioned via centerValueUnitGroup() so the [value][unit] pair centers on
// `x + width / 2` instead of hugging the box's edge — `representativeValue`
// must be the same "widest expected value" string passed to the paired
// drawValueBoxValue() call (see reservedValueWidth()'s doc). Call once from
// drawStatic()/drawConfigXStatic(); pair with drawValueBoxValue() from the
// matching drawDynamic() using the same x/y/width/unit/representativeValue/
// valueTextSize/labelToValueGap.
void drawValueBoxStatic(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, const char* label,
                         const char* unit, const char* representativeValue, const ThemeColors& theme,
                         uint8_t labelTextSize = 1, uint8_t valueTextSize = 2, int32_t labelToValueGap = 14);

// Dynamic half of a unit-bearing value box: draws only the number, right-
// aligned within the reserved value slot computed from
// `representativeValue` (must match the string passed to the paired
// drawValueBoxStatic() call) so it lines up against the unit drawn there.
// Never touches the label or the unit.
void drawValueBoxValue(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, const char* formattedValue,
                        const char* unit, const char* representativeValue, bool valid, const ThemeColors& theme,
                        uint8_t valueTextSize = 2, int32_t labelToValueGap = 14);

// drawString() repaints only the glyph box of the new string, so a readout that
// loses a character leaves the previous, wider one's outer columns on screen. Use
// this wherever the rendered width can change between frames: it blanks only the
// margins between the text and its `fieldWidth` slot, so a steady-state frame
// repaints no lit pixels and cannot flicker. x/y and alignment follow the current
// text datum exactly as drawString() would.
void drawFieldText(TFT_eSPI& tft, const char* text, int32_t x, int32_t y,
                    int32_t fieldWidth, uint16_t background);

// Status strip badge (CONNECTING / LIVE / STALE / NO OBD / SD OFFLINE, etc).
void drawStatusBadge(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, int32_t height,
                      const char* text, uint16_t badgeColor, uint16_t textColor);

// MIL (check engine light) indicator; also the Page 5 touch zone glyph.
void drawMilIndicator(TFT_eSPI& tft, int32_t centerX, int32_t centerY, bool milOn, const ThemeColors& theme);

// Dashboard/config-group mode toggle glyph: a gear while `showGear` is true
// (on a dashboard page - tap to go to config), a steering wheel otherwise (on
// a config page - tap to return to the dashboard).
void drawModeToggleButton(TFT_eSPI& tft, int32_t centerX, int32_t centerY, bool showGear, const ThemeColors& theme);

} // namespace gaugewidgets
