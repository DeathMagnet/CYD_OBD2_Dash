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
// text redraw to avoid flicker, matching the pattern already used by
// DisplayManager::drawHelloWorld.
namespace gaugewidgets {

// Lets an arc gauge repaint only the sweep that actually moved between frames.
// Call invalidate() whenever the area behind the gauge is cleared (page redraw).
struct ArcGaugeState {
    float lastValueAngle = 0.0F;
    float lastMaxValue = 0.0F;
    bool needsFullRedraw = true;

    void invalidate() { needsFullRedraw = true; }
};

// Circular arc gauge, 0..maxValue swept from 30 degrees to 330 degrees
// (bottom-left to bottom-right, opening at 6 o'clock) per TFT_eSPI's
// drawSmoothArc angle convention. `bgColor` must match the surface color
// immediately behind the gauge so anti-aliased edges blend correctly. The
// redline gradient starts at redlineStart, reaches the full redline color at
// redlineEnd, then holds it out to the end of the arc; pass
// redlineStart >= redlineEnd (e.g. both equal to maxValue) to disable it for
// gauges that don't have a redline concept (load, vacuum/boost, etc).
void drawArcGauge(TFT_eSPI& tft, ArcGaugeState& state, int32_t centerX, int32_t centerY, int32_t radius,
                   float value, float maxValue, float redlineStart, float redlineEnd, uint16_t bgColor,
                   const ThemeColors& theme);

// Flat horizontal bar (0-100%) with outline and fill, used for
// throttle/load/trim style readouts.
void drawBarGauge(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, int32_t height,
                   float percent, uint16_t fillColor, const ThemeColors& theme);

// A caption above a large numeric readout. Renders "--" in textSecondary
// when !valid rather than a fabricated zero (see coding standards).
void drawValueBox(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width,
                   const char* label, const char* formattedValue, bool valid, const ThemeColors& theme);

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

// MIL (check engine light) indicator; also the Page 6 touch zone glyph.
void drawMilIndicator(TFT_eSPI& tft, int32_t centerX, int32_t centerY, bool milOn, const ThemeColors& theme);

} // namespace gaugewidgets
