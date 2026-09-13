#include "display/theme.h"
#include "labels.h"

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
    0x262B, // unsavedActive: green (Save button, unsaved config changes)
    0x262B, // liveActive: green (OBDII badge, connection live)
    labels::kThemeNameModernFlat,
};

// Torque Neon: high-tech digital look inspired by the Torque Pro Android app
// (see docs/cyd-obd2-ui-cluster-guide.md). Black background, saturated neon
// gauge arcs, hot-orange warnings.
constexpr ThemeColors kTorqueNeonTheme = {
    0x0000, // background: black
    0x0862, // panel: near-black blue card surface
    0x07E0, // primaryGaugeArc: neon green
    0x07FF, // secondaryGaugeArc: electric cyan
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
    labels::kThemeNameTorqueNeon,
};

} // namespace

const ThemeColors& getTheme(ThemeId id) {
    switch (id) {
        case ThemeId::TorqueNeon: return kTorqueNeonTheme;
        // Mustang S197 is not implemented yet (deliberately deferred); it
        // falls back to Modern Flat so callers always get a complete,
        // correct palette.
        case ThemeId::MustangS197:
        case ThemeId::ModernFlat:
        default: return kModernFlatTheme;
    }
}
