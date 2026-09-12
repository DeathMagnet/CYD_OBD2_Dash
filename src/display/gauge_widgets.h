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
        float force = (target - currentValue) * stiffness;
        float dampingForce = velocity * damping;
        float accel = force - dampingForce;
        velocity += accel * dtSeconds;
        currentValue += velocity * dtSeconds;
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

// Circular arc gauge, 0..maxValue swept from 30 degrees to 330 degrees
// (bottom-left to bottom-right, opening at 6 o'clock) per TFT_eSPI's
// drawSmoothArc angle convention. `bgColor` must match the surface color
// immediately behind the gauge so anti-aliased edges blend correctly. The
// redline gradient renders between redlineStart and redlineEnd; pass
// redlineStart >= redlineEnd (e.g. both equal to maxValue) to disable it for
// gauges that don't have a redline concept (load, vacuum/boost, etc).
void drawArcGauge(TFT_eSPI& tft, int32_t centerX, int32_t centerY, int32_t radius,
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

// Status strip badge (CONNECTING / LIVE / STALE / NO OBD / SD OFFLINE, etc).
void drawStatusBadge(TFT_eSPI& tft, int32_t x, int32_t y, int32_t width, int32_t height,
                      const char* text, uint16_t badgeColor, uint16_t textColor);

// MIL (check engine light) indicator; also the Page 6 touch zone glyph.
void drawMilIndicator(TFT_eSPI& tft, int32_t centerX, int32_t centerY, bool milOn, const ThemeColors& theme);

} // namespace gaugewidgets
