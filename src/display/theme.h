#pragma once

#include <stdint.h>

// Three themes are implemented (docs/cyd-obd2-ui-cluster-guide.md): Mustang
// S197, Torque Neon, and Modern Flat.
enum class ThemeId : uint8_t {
    MustangS197 = 0,
    TorqueNeon = 1,
    ModernFlat = 2,
};

struct ThemeColors {
    uint16_t background;
    uint16_t panel;
    uint16_t primaryGaugeArc;
    uint16_t secondaryGaugeArc;
    uint16_t tickInactiveColor; // Tick color before the needle reaches it (independent of secondaryGaugeArc).
    uint16_t needle;
    uint16_t needleCap;
    uint16_t cautionArc;
    uint16_t dangerArc;
    uint16_t bezel;
    uint16_t textPrimary;
    uint16_t textSecondary;
    uint16_t warningActive;
    uint16_t touchHighlight;
    uint16_t unsavedActive; // Save button fill while a config page has unsaved changes.
    uint16_t liveActive;    // OBDII status badge while ConnectionState::Live.
    bool showGaugeBezel;    // Draw a decorative chrome ring around round gauges.
    bool showOuterTicks;    // Draw the tick segment outside the gauge ring too (inner segment always drawn).
    bool useSevenSegmentFont; // Use TFT_eSPI's built-in Font 7 (7-segment LED look) for large text (TextSize > 2).
    const char* name;
};

const ThemeColors& getTheme(ThemeId id);
