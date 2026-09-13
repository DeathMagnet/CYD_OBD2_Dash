#include "system/config_store.h"
#include <Arduino.h>
#include <SD.h>
#include <string.h>
#include <stdlib.h>

namespace {

uint16_t clampU16(long value, uint16_t lo, uint16_t hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return static_cast<uint16_t>(value);
}

float clampF(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

bool isValidLogIntervalMs(uint32_t intervalMs) {
    for (size_t i = 0; i < config::kLogRowIntervalOptionCount; ++i) {
        if (config::kLogRowIntervalOptionsMs[i] == intervalMs) {
            return true;
        }
    }
    return false;
}

} // namespace

ConfigStore::ConfigStore(SdManager& sdManager) : sdManager_(sdManager) {}

bool ConfigStore::parseLine(const char* line) {
    const char* equalsSign = strchr(line, '=');
    if (equalsSign == nullptr) {
        return false;
    }

    char key[32];
    size_t keyLen = static_cast<size_t>(equalsSign - line);
    if (keyLen >= sizeof(key)) {
        return false;
    }
    memcpy(key, line, keyLen);
    key[keyLen] = '\0';

    const char* valueStr = equalsSign + 1;

    if (strcmp(key, "shift_light_rpm") == 0) {
        settings_.shiftLightRpm = clampU16(atol(valueStr), config::kMinShiftLightRpm, config::kMaxShiftLightRpm);
    } else if (strcmp(key, "redline_rpm") == 0) {
        settings_.redlineRpm = clampU16(atol(valueStr), config::kMinRedlineRpm, config::kMaxRedlineRpm);
    } else if (strcmp(key, "max_rpm") == 0) {
        settings_.maxRpm = clampU16(atol(valueStr), config::kMinMaxRpm, config::kMaxMaxRpm);
    } else if (strcmp(key, "max_speed_mph") == 0) {
        settings_.maxSpeedMph = clampU16(atol(valueStr), config::kMinMaxSpeedMph, config::kMaxMaxSpeedMph);
    } else if (strcmp(key, "log_interval_ms") == 0) {
        uint32_t interval = static_cast<uint32_t>(atol(valueStr));
        settings_.logIntervalMs = isValidLogIntervalMs(interval) ? interval : config::kDefaultLogRowIntervalMs;
    } else if (strcmp(key, "baro_baseline_psi") == 0) {
        settings_.baroBaselinePsi = clampF(static_cast<float>(atof(valueStr)), config::kMinBaroBaselinePsi, config::kMaxBaroBaselinePsi);
    } else if (strcmp(key, "theme") == 0) {
        long theme = atol(valueStr);
        settings_.themeId = (theme >= 0 && theme <= 2) ? static_cast<uint8_t>(theme) : 2;
    } else if (strcmp(key, "units") == 0) {
        settings_.useMetricUnits = (atol(valueStr) != 0);
    } else if (strcmp(key, "log_units") == 0) {
        settings_.useMetricLogs = (atol(valueStr) != 0);
    } else {
        return false;
    }
    return true;
}

void ConfigStore::begin() {
    settings_ = AppSettings();

    if (!sdManager_.isMounted() || !SD.exists(config::kConfigFilePath)) {
        Serial.println("[Config] No /config.txt found; using defaults.");
        return;
    }

    File file = SD.open(config::kConfigFilePath, FILE_READ);
    if (!file) {
        Serial.println("[Config] Failed to open /config.txt; using defaults.");
        return;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) {
            continue;
        }
        parseLine(line.c_str());
    }
    file.close();

    // parseLine() only clamps each field to its own independent bounds, so a
    // stale or hand-edited file can leave the shiftLight <= redline <= maxRpm
    // ordering broken regardless of key order; fix it up top-down once here.
    if (settings_.redlineRpm > settings_.maxRpm) {
        settings_.redlineRpm = settings_.maxRpm;
    }
    if (settings_.shiftLightRpm > settings_.redlineRpm) {
        settings_.shiftLightRpm = settings_.redlineRpm;
    }

    Serial.printf("[Config] Loaded: shiftLight=%u redline=%u maxRpm=%u maxSpeed=%u logIntervalMs=%lu baroBaselinePsi=%.2f theme=%u\n",
                  settings_.shiftLightRpm, settings_.redlineRpm, settings_.maxRpm, settings_.maxSpeedMph,
                  static_cast<unsigned long>(settings_.logIntervalMs), settings_.baroBaselinePsi, settings_.themeId);

    savedSettings_ = settings_;
}

void ConfigStore::setShiftLightRpm(uint16_t rpm) {
    // Clamped below redline so shiftLight <= redline always holds; each
    // setter only ever tightens its own field, never a sibling's.
    uint16_t hi = config::kMaxShiftLightRpm < settings_.redlineRpm ? config::kMaxShiftLightRpm : settings_.redlineRpm;
    settings_.shiftLightRpm = clampU16(rpm, config::kMinShiftLightRpm, hi);
}

void ConfigStore::setRedlineRpm(uint16_t rpm) {
    uint16_t lo = config::kMinRedlineRpm > settings_.shiftLightRpm ? config::kMinRedlineRpm : settings_.shiftLightRpm;
    uint16_t hi = config::kMaxRedlineRpm < settings_.maxRpm ? config::kMaxRedlineRpm : settings_.maxRpm;
    settings_.redlineRpm = clampU16(rpm, lo, hi);
}

void ConfigStore::setMaxRpm(uint16_t rpm) {
    uint16_t lo = config::kMinMaxRpm > settings_.redlineRpm ? config::kMinMaxRpm : settings_.redlineRpm;
    settings_.maxRpm = clampU16(rpm, lo, config::kMaxMaxRpm);
}

void ConfigStore::setMaxSpeedMph(uint16_t mph) {
    settings_.maxSpeedMph = clampU16(mph, config::kMinMaxSpeedMph, config::kMaxMaxSpeedMph);
}

void ConfigStore::setLogIntervalMs(uint32_t intervalMs) {
    settings_.logIntervalMs = isValidLogIntervalMs(intervalMs) ? intervalMs : config::kDefaultLogRowIntervalMs;
}

void ConfigStore::setBaroBaselinePsi(float psi) {
    settings_.baroBaselinePsi = clampF(psi, config::kMinBaroBaselinePsi, config::kMaxBaroBaselinePsi);
}

void ConfigStore::setUseMetricUnits(bool metric) {
    settings_.useMetricUnits = metric;
}

void ConfigStore::setUseMetricLogs(bool metric) {
    settings_.useMetricLogs = metric;
}

bool ConfigStore::isDirty() const {
    return settings_.shiftLightRpm != savedSettings_.shiftLightRpm ||
           settings_.redlineRpm != savedSettings_.redlineRpm ||
           settings_.maxRpm != savedSettings_.maxRpm ||
           settings_.maxSpeedMph != savedSettings_.maxSpeedMph ||
           settings_.logIntervalMs != savedSettings_.logIntervalMs ||
           settings_.baroBaselinePsi != savedSettings_.baroBaselinePsi ||
           settings_.themeId != savedSettings_.themeId ||
           settings_.useMetricUnits != savedSettings_.useMetricUnits ||
           settings_.useMetricLogs != savedSettings_.useMetricLogs;
}

bool ConfigStore::isLogUnitsDirty() const {
    return settings_.useMetricLogs != savedSettings_.useMetricLogs;
}

bool ConfigStore::save() {
    if (!sdManager_.isMounted()) {
        Serial.println("[Config] Cannot save: SD not mounted.");
        return false;
    }

    // FILE_WRITE on this SD implementation seeks to end-of-file rather than
    // truncating, so remove any previous version before writing a fresh one.
    if (SD.exists(config::kConfigFilePath)) {
        SD.remove(config::kConfigFilePath);
    }

    File file = SD.open(config::kConfigFilePath, FILE_WRITE);
    if (!file) {
        Serial.println("[Config] Failed to open /config.txt for writing.");
        return false;
    }

    file.printf("theme=%u\n", settings_.themeId);
    file.printf("shift_light_rpm=%u\n", settings_.shiftLightRpm);
    file.printf("redline_rpm=%u\n", settings_.redlineRpm);
    file.printf("max_rpm=%u\n", settings_.maxRpm);
    file.printf("max_speed_mph=%u\n", settings_.maxSpeedMph);
    file.printf("log_interval_ms=%lu\n", static_cast<unsigned long>(settings_.logIntervalMs));
    file.printf("baro_baseline_psi=%.2f\n", settings_.baroBaselinePsi);
    file.printf("units=%u\n", settings_.useMetricUnits ? 1 : 0);
    file.printf("log_units=%u\n", settings_.useMetricLogs ? 1 : 0);
    file.flush();
    file.close();

    Serial.println("[Config] Settings saved to /config.txt.");
    savedSettings_ = settings_;
    return true;
}
