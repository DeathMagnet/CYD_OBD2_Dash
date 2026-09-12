#include "display/theme.h"

namespace {

// Modern Flat: minimalist EV/performance HUD look (see
// docs/cyd-obd2-ui-cluster-guide.md). Slate background, crisp white gauge
// faces, accent blue arcs, crimson warning badges.
constexpr ThemeColors kModernFlatTheme = {
    0x18C3, // background: slate gray
    0x2965, // panel: slightly lighter slate for card surfaces
    0x03FF, // primaryGaugeArc: accent blue
    0x7BEF, // secondaryGaugeArc: muted gray-blue
    0xFFFF, // needle: crisp white
    0x03FF, // needleCap: accent blue
    0xFD20, // cautionArc: orange (Shift Light RPM -> Redline RPM zone)
    0xF800, // dangerArc: red (Redline RPM -> end of sweep)
    0xC618, // bezel: light silver
    0xFFFF, // textPrimary: white
    0xC618, // textSecondary: silver
    0xD800, // warningActive: crimson
    0x5D1F, // touchHighlight: light blue
    "Modern Flat",
};

} // namespace

const ThemeColors& getTheme(ThemeId /*id*/) {
    // Mustang S197 and Torque Neon are not implemented yet (deliberately
    // deferred); every theme request resolves to Modern Flat until they are
    // built, so callers always get a complete, correct palette.
    return kModernFlatTheme;
}
