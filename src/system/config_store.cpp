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
    } else if (strcmp(key, "log_interval_ms") == 0) {
        uint32_t interval = static_cast<uint32_t>(atol(valueStr));
        settings_.logIntervalMs = isValidLogIntervalMs(interval) ? interval : config::kDefaultLogRowIntervalMs;
    } else if (strcmp(key, "baro_baseline_psi") == 0) {
        settings_.baroBaselinePsi = clampF(static_cast<float>(atof(valueStr)), config::kMinBaroBaselinePsi, config::kMaxBaroBaselinePsi);
    } else if (strcmp(key, "theme") == 0) {
        long theme = atol(valueStr);
        settings_.themeId = (theme >= 0 && theme <= 2) ? static_cast<uint8_t>(theme) : 2;
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

    Serial.printf("[Config] Loaded: shiftLight=%u redline=%u logIntervalMs=%lu baroBaselinePsi=%.2f theme=%u\n",
                  settings_.shiftLightRpm, settings_.redlineRpm,
                  static_cast<unsigned long>(settings_.logIntervalMs),
                  settings_.baroBaselinePsi, settings_.themeId);

    savedSettings_ = settings_;
}

void ConfigStore::setShiftLightRpm(uint16_t rpm) {
    settings_.shiftLightRpm = clampU16(rpm, config::kMinShiftLightRpm, config::kMaxShiftLightRpm);
}

void ConfigStore::setRedlineRpm(uint16_t rpm) {
    settings_.redlineRpm = clampU16(rpm, config::kMinRedlineRpm, config::kMaxRedlineRpm);
}

void ConfigStore::setLogIntervalMs(uint32_t intervalMs) {
    settings_.logIntervalMs = isValidLogIntervalMs(intervalMs) ? intervalMs : config::kDefaultLogRowIntervalMs;
}

void ConfigStore::setBaroBaselinePsi(float psi) {
    settings_.baroBaselinePsi = clampF(psi, config::kMinBaroBaselinePsi, config::kMaxBaroBaselinePsi);
}

bool ConfigStore::isDirty() const {
    return settings_.shiftLightRpm != savedSettings_.shiftLightRpm ||
           settings_.redlineRpm != savedSettings_.redlineRpm ||
           settings_.logIntervalMs != savedSettings_.logIntervalMs ||
           settings_.baroBaselinePsi != savedSettings_.baroBaselinePsi ||
           settings_.themeId != savedSettings_.themeId;
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
    file.printf("log_interval_ms=%lu\n", static_cast<unsigned long>(settings_.logIntervalMs));
    file.printf("baro_baseline_psi=%.2f\n", settings_.baroBaselinePsi);
    file.flush();
    file.close();

    Serial.println("[Config] Settings saved to /config.txt.");
    savedSettings_ = settings_;
    return true;
}
