#pragma once

#include <stdint.h>

// Three themes are planned (docs/cyd-obd2-ui-cluster-guide.md); only Modern
// Flat is implemented today. The enum exists now so Page 5's theme setting
// and ConfigStore's persisted value are forward-compatible once Mustang
// S197 and Torque Neon are built.
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
    const char* name;
};

// Mustang S197 and Torque Neon are not implemented yet; requesting either
// currently returns the Modern Flat palette so callers always get a
// complete, correct theme rather than an undefined one.
const ThemeColors& getTheme(ThemeId id);
