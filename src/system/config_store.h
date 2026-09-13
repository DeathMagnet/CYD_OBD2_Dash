#pragma once

#include <stdint.h>
#include "app_config.h"
#include "storage/sd_manager.h"

// User-adjustable dashboard settings, persisted to /config.txt on the SD
// card as plain "key=value" lines (see docs/cyd-obd2-ui-cluster-guide.md,
// Page 5). Kept as flat key=value text rather than a JSON library: this is a
// handful of scalars, and the coding standards prefer plain direct code over
// pulling in a parsing dependency for a problem this small.
struct AppSettings {
    uint16_t shiftLightRpm = config::kDefaultShiftLightRpm;
    uint16_t redlineRpm = config::kDefaultRedlineRpm;
    uint32_t logIntervalMs = config::kDefaultLogRowIntervalMs;
    float baroBaselinePsi = config::kDefaultBaroBaselinePsi;
    // Theme selection is persisted for forward compatibility, but only
    // Modern Flat (2) renders today; Mustang S197 (0) and Torque Neon (1)
    // are reserved until those themes are implemented.
    uint8_t themeId = 2;
};

class ConfigStore {
public:
    explicit ConfigStore(SdManager& sdManager);

    // Loads /config.txt if present; keeps built-in defaults for any key
    // that's missing, malformed, or out of range.
    void begin();

    const AppSettings& settings() const { return settings_; }

    void setShiftLightRpm(uint16_t rpm);
    void setRedlineRpm(uint16_t rpm);
    void setLogIntervalMs(uint32_t intervalMs);
    void setBaroBaselinePsi(float psi);

    // True whenever the current settings differ from the last saved/loaded
    // snapshot (a live comparison, not a sticky flag - reverting a value back
    // to what's on SD clears this again). Drives the shared config-page Save
    // button's color.
    bool isDirty() const;

    // Persists the current settings to /config.txt. Returns false if the SD
    // card is unavailable or the write fails; clears isDirty() on success.
    bool save();

private:
    bool parseLine(const char* line);

    SdManager& sdManager_;
    AppSettings settings_;
    AppSettings savedSettings_; // Snapshot of what's currently on SD, for isDirty().
};
