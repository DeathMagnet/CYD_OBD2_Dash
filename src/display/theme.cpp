#include "display/theme.h"
#include "labels.h"
#include "display/fonts/Orbitron_Light_24.h"
#include "display/fonts/Orbitron_Light_32.h"

extern const GFXfont FreeSans9pt7b;
extern const GFXfont FreeSans12pt7b;
extern const GFXfont FreeSans18pt7b;
extern const GFXfont FreeSans24pt7b;

namespace {

// Modern Flat: minimalist EV/performance HUD look (see
// docs/cyd-obd2-ui-cluster-guide.md). Slate background, crisp white gauge
// faces, accent blue arcs, crimson warning badges.
ThemeColors kModernFlatTheme = {
    0x18C3, // background: slate gray
    0x2965, // panel: slightly lighter slate for card surfaces
    0x03FF, // primaryGaugeArc: accent blue
    0x7BEF, // secondaryGaugeArc: muted gray-blue
    0x7BEF, // tickInactiveColor: muted gray-blue (matches secondaryGaugeArc)
    0xFFFF, // needle: crisp white
    0x03FF, // needleCap: accent blue
    0xFD20, // cautionArc: orange (Shift Light RPM -> Redline RPM zone)
    0xF800, // dangerArc: red (Redline RPM -> end of sweep)
    0xC618, // bezel: light silver
    0xFFFF, // textPrimary: white
    0xC618, // textSecondary: silver
    0xD800, // warningActive: crimson
    0x5D1F, // touchHighlight: light blue
    0x262B, // unsavedActive: green (Save button, unsaved config changes)
    0x262B, // liveActive: green (OBDII badge, connection live)
    false,  // showGaugeBezel
    true,   // showOuterTicks
    false,  // useSevenSegmentFont
    false,  // useSegmentedBars
    false,  // useSegmentedArcs
    labels::kThemeNameModernFlat,
    {},     // valueFonts: initialized via initializeThemeFonts()
    {},     // numberedFonts: not used
};

// Neon: high-tech digital look inspired by the Torque Pro Android app
// (see docs/cyd-obd2-ui-cluster-guide.md). Black background, saturated neon
// gauge arcs, hot-orange warnings.
ThemeColors kNeonTheme = {
    0x0000, // background: black
    0x0862, // panel: near-black blue card surface
    0x07E0, // primaryGaugeArc: neon green
    0x07FF, // secondaryGaugeArc: electric cyan
    0x07FF, // tickInactiveColor: electric cyan (matches secondaryGaugeArc)
    0xFFFF, // needle: crisp white
    0xF81F, // needleCap: neon magenta accent
    0xFFE0, // cautionArc: neon yellow (Shift Light RPM -> Redline RPM zone)
    0xF800, // dangerArc: pure red (Redline RPM -> end of sweep)
    0x2987, // bezel: dim steel-blue outline
    0xFFFF, // textPrimary: white
    0x7D56, // textSecondary: soft cyan-gray
    0xFDA0, // warningActive: hot orange
    0xF81F, // touchHighlight: neon magenta
    0x07E0, // unsavedActive: neon green (Save button, unsaved config changes)
    0x07E0, // liveActive: neon green (OBDII badge, connection live)
    false,  // showGaugeBezel
    true,   // showOuterTicks
    false,  // useSevenSegmentFont
    false,  // useSegmentedBars
    false,  // useSegmentedArcs
    labels::kThemeNameNeon,
    {},     // valueFonts: initialized via initializeThemeFonts()
    {},     // numberedFonts: not used
};

// S197: OEM 2005-2010 Ford Mustang instrument cluster look (see
// docs/cyd-obd2-ui-cluster-guide.md). Deep midnight background, LED-green
// gauge arcs and text, chrome bezels, vibrant red needle.
constexpr ThemeColors kS197Theme = {
    0x0821, // background: deep midnight navy
    0x10A5, // panel: slightly lighter navy card surface
    0x001F, // primaryGaugeArc: LED blue
    0x39C7, // secondaryGaugeArc: dark grey
    0xC618, // tickInactiveColor: silver (lights up LED blue once the needle passes)
    0xF800, // needle: vibrant red
    0xC618, // needleCap: chrome hub
    0xFD20, // cautionArc: amber (Shift Light RPM -> Redline RPM zone)
    0xF800, // dangerArc: red (Redline RPM -> end of sweep)
    0xC618, // bezel: metallic chrome
    0x07E0, // textPrimary: LED green
    0x5D8D, // textSecondary: soft LED green
    0xF8C0, // warningActive: amber-orange
    0x559F, // touchHighlight: light ice-blue
    0x062B, // unsavedActive: green (Save button, unsaved config changes)
    0x062B, // liveActive: green (OBDII badge, connection live)
    true,   // showGaugeBezel: OEM chrome ring around round gauges
    false,  // showOuterTicks: only the inner tick segment is shown
    true,   // useSevenSegmentFont: TFT_eSPI Font 7 for large readouts
    true,   // useSegmentedBars: OEM-style segmented LED bar look
    true,   // useSegmentedArcs: OEM-style segmented LED ring look
    labels::kThemeNameS197,
    {},     // valueFonts: nullptr array (use default GLCD font)
    {0, 7, 7}, // numberedFonts: Font 7 (7-segment LCD) for tiers 3/4 (boost/vacuum and RPM/Speed); tier 2 (drawValueBox) stays on default
};

} // namespace

const ThemeColors& getTheme(ThemeId id) {
    switch (id) {
        case ThemeId::Neon: return kNeonTheme;
        case ThemeId::S197: return kS197Theme;
        case ThemeId::ModernFlat:
        default: return kModernFlatTheme;
    }
}

void applyValueFont(TFT_eSPI& tft, const ThemeColors& theme, uint8_t size) {
    if (size < 2 || size > 5) {
        return;
    }
    int idx = size - 2;

    // Check free fonts first (GFXFF)
    const GFXfont* font = theme.valueFonts[idx];
    if (font != nullptr) {
        tft.setFreeFont(font);
        tft.setTextSize(1);
        return;
    }

    // Check numbered fonts (Font 1-8)
    uint8_t numberedFont = theme.numberedFonts[idx];
    if (numberedFont > 0) {
        tft.setTextFont(numberedFont);
        tft.setTextSize(1);
        return;
    }

    if (size == 5) {
        // No dedicated tier-5 override for this theme (e.g. S197): fall back
        // to tier 3 instead of an oversized default GLCD font.
        applyValueFont(tft, theme, 3);
        return;
    }

    // Fall back to default: Font 1 at the original size tier
    tft.setTextFont(1);
    tft.setTextSize(size);
}

void resetValueFont(TFT_eSPI& tft) {
    tft.setTextFont(1);
}

void applyLabelFont(TFT_eSPI& tft) {
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextSize(1);
}

void applyConfigValueFont(TFT_eSPI& tft) {
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextSize(1);
}

void initializeThemeFonts() {
    kModernFlatTheme.valueFonts[0] = &FreeSans12pt7b;
    kModernFlatTheme.valueFonts[1] = &FreeSans18pt7b;
    kModernFlatTheme.valueFonts[2] = &FreeSans24pt7b;

    kNeonTheme.valueFonts[0] = &Orbitron_Light_24_Fixed;
    kNeonTheme.valueFonts[1] = &Orbitron_Light_24_Fixed;
    kNeonTheme.valueFonts[2] = &Orbitron_Light_32_Fixed;

    // Tier 5: dedicated, larger Engine Load value font (reuses the RPM/Speed
    // tier-4 font) without changing Boost/Vacuum's shared tier-3 size.
    kModernFlatTheme.valueFonts[3] = &FreeSans24pt7b;
    kNeonTheme.valueFonts[3] = &Orbitron_Light_32_Fixed;
}
