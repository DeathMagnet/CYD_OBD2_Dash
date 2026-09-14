#pragma once

#include <stdint.h>
#include "app_config.h"
#include "display/theme.h"
#include "storage/sd_manager.h"

// User-adjustable dashboard settings, persisted to /config.txt on the SD
// card as plain "key=value" lines (see docs/cyd-obd2-ui-cluster-guide.md,
// "Config Pages" section). Kept as flat key=value text rather than a JSON
// library: this is a handful of scalars, and the coding standards prefer plain
// direct code over pulling in a parsing dependency for a problem this small.
struct AppSettings {
    uint16_t shiftLightRpm = config::kDefaultShiftLightRpm;
    uint16_t redlineRpm = config::kDefaultRedlineRpm;
    uint16_t maxRpm = config::kDefaultMaxRpm;
    uint16_t maxSpeedMph = config::kDefaultMaxSpeedMph;
    uint32_t logIntervalMs = config::kDefaultLogRowIntervalMs;
    float baroBaselinePsi = config::kDefaultBaroBaselinePsi;
    float coolantWarningF = config::kDefaultCoolantWarningF;
    float lowVoltageWarningV = config::kDefaultLowVoltageWarningV;
    float boostMaxPsi = config::kDefaultBoostMaxPsi;
    float vacuumMaxInHg = config::kDefaultVacuumMaxInHg;
    float zeroSixtyTargetMph = config::kDefaultZeroSixtyTargetMph;
    float hpEstimationFactor = config::kDefaultHpEstimationFactor;
    float fuelTrimRangePct = config::kDefaultFuelTrimRangePct;
    // Theme selection is persisted to /config.txt; see display/theme.h for
    // the ThemeId values (S197=0, Neon=1, ModernFlat=2).
    uint8_t themeId = config::kDefaultThemeId;
    bool useMetricUnits = false; // Display units (dashboard pages, config fields)
    bool useMetricLogs = false;  // CSV logging units
    // Page 1 RPM/Speed arc tick marks (TickMode, display/theme.h). S197
    // only supports Off/InsideOnly; see ConfigStore::clampTickModeForTheme.
    uint8_t tickMode = config::kDefaultTickMode;

    // ELM327 Bluetooth identity, picked from config::kObdAdapterNameOptions/
    // kObdAdapterPinOptions on the Config: OBD Adapter page. Char arrays can't
    // use the "= config::kDefaultX" style the fields above use, so they're
    // defaulted in the constructor instead.
    char obdAdapterName[24];
    char obdAdapterPin[9];

    AppSettings();
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
    void setMaxRpm(uint16_t rpm);
    void setMaxSpeedMph(uint16_t mph);
    void setLogIntervalMs(uint32_t intervalMs);
    void setBaroBaselinePsi(float psi);
    void setCoolantWarningF(float f);
    void setLowVoltageWarningV(float v);
    void setBoostMaxPsi(float psi);
    void setVacuumMaxInHg(float inHg);
    void setZeroSixtyTargetMph(float mph);
    void setHpEstimationFactor(float factor);
    void setFuelTrimRangePct(float pct);
    void setThemeId(uint8_t id);
    void setUseMetricUnits(bool metric);
    void setUseMetricLogs(bool metric);
    void setTickMode(uint8_t mode);
    void setObdAdapterName(const char* name);
    void setObdAdapterPin(const char* pin);

    // True whenever the current settings differ from the last saved/loaded
    // snapshot (a live comparison, not a sticky flag - reverting a value back
    // to what's on SD clears this again). Drives the shared config-page Save
    // button's color.
    bool isDirty() const;

    // True if useMetricLogs has changed since the last save; used to gate
    // whether log files get wiped when Save is tapped (wipe only once, only if
    // it actually changed relative to what's on SD).
    bool isLogUnitsDirty() const;

    // True if the OBD adapter name or PIN has changed since the last save;
    // used to gate the live Bluetooth reconnect when Save is tapped (only
    // reconnect if the credentials actually changed relative to what's on SD).
    bool isObdCredentialsDirty() const;

    // Persists the current settings to /config.txt. Returns false if the SD
    // card is unavailable or the write fails; clears isDirty() on success.
    bool save();

private:
    bool parseLine(const char* line);

    // Forces tickMode down to InsideOnly if it's currently Outside/
    // InsideAndOutside while the active theme is S197, which has no
    // room in its bezel art for outside ticks. Called after any change to
    // themeId or tickMode, and once after loading from SD.
    void clampTickModeForTheme();

    SdManager& sdManager_;
    AppSettings settings_;
    AppSettings savedSettings_; // Snapshot of what's currently on SD, for isDirty().
};
