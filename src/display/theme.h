#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>

// Three themes are implemented (docs/cyd-obd2-ui-cluster-guide.md): S197,
// Neon, and Modern Flat.
enum class ThemeId : uint8_t {
    S197 = 0,
    Neon = 1,
    ModernFlat = 2,
};

// RPM/Speed gauge tick display mode (Config: UI page). S197 cannot
// render outside ticks (ThemeColors::showOuterTicks is false), so only Off
// and InsideOnly are valid while that theme is active.
enum class TickMode : uint8_t {
    Off = 0,
    InsideOnly = 1,
    OutsideOnly = 2,
    InsideAndOutside = 3,
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
    bool useSegmentedBars;    // Render bar gauges as a segmented LED/VU-meter style
                              // (discrete lit blocks) instead of a smooth continuous fill.
    bool useSegmentedArcs;    // Render round gauges as a segmented LED ring style
                              // (discrete lit wedges) instead of a smooth continuous arc.
    const char* name;
    const GFXfont* valueFonts[4]; // Fonts for setTextSize(2/3/4/5); indexed by (size - 2). nullptr = use default GLCD font.
                                   // Tier 5 (index 3) is reserved for Engine Load's optionally-enlarged value; a
                                   // theme that leaves it unset falls back to tier 3 (see applyValueFont()).
    uint8_t numberedFonts[4]; // Numbered fonts (Font 1-8) for setTextSize(2/3/4/5). 0 = no override (use default); else font number passed to setTextFont().
};

const ThemeColors& getTheme(ThemeId id);

void initializeThemeFonts();
void applyValueFont(TFT_eSPI& tft, const ThemeColors& theme, uint8_t size);
void resetValueFont(TFT_eSPI& tft);

// Config-page setting labels always use a small proportional font
// (FreeSans9pt7b - a smaller sibling of Modern Flat's FreeSans12pt7b value
// font, chosen so the longest label strings fit their column), regardless of
// the active theme - unlike applyValueFont(), which picks a theme-specific
// font for gauge values. Callers must pair this with resetValueFont()
// immediately after drawing the label, since GFX font
// selection is sticky and every other config-page draw call assumes Font 1.
void applyLabelFont(TFT_eSPI& tft);

// Config-page setting *values* (numbers, not free text) use the same face and
// size as applyLabelFont() - FreeSans9pt7b at size 1 - kept as its own named
// function so value-widget font policy can still be tuned independently of
// label font policy later. Like applyLabelFont(), callers must pair this with
// resetValueFont() immediately after drawing since GFX font selection is
// sticky and every other config-page draw call assumes Font 1.
void applyConfigValueFont(TFT_eSPI& tft);
