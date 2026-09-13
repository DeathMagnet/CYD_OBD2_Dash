#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>

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
    const char* name;
    const GFXfont* valueFonts[3]; // Fonts for setTextSize(2/3/4); indexed by (size - 2). nullptr = use default GLCD font.
    uint8_t numberedFonts[3]; // Numbered fonts (Font 1-8) for setTextSize(2/3/4). 0 = no override (use default); else font number passed to setTextFont().
};

const ThemeColors& getTheme(ThemeId id);

void initializeThemeFonts();
void applyValueFont(TFT_eSPI& tft, const ThemeColors& theme, uint8_t size);
void resetValueFont(TFT_eSPI& tft);
