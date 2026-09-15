#include "obd/obd_credentials.h"
#include <Arduino.h>
#include <SD.h>
#include <string.h>
#include "app_config.h"
#include "storage/sd_manager.h"

namespace {

// Accepts any non-hex character as a separator (":", "-", or none at all),
// so "AA:BB:CC:DD:EE:FF", "AA-BB-CC-DD-EE-FF", and "AABBCCDDEEFF" all parse.
// Fails (leaving `out` untouched) on anything but exactly 12 hex digits.
bool parseMacAddress(const char* str, uint8_t out[6]) {
    uint8_t parsed[6];
    uint8_t byteIndex = 0;
    uint8_t nibbleCount = 0;
    uint8_t currentByte = 0;

    for (const char* p = str; *p != '\0'; ++p) {
        char c = *p;
        int nibble;
        if (c >= '0' && c <= '9') {
            nibble = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            nibble = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            nibble = c - 'A' + 10;
        } else {
            continue;
        }

        currentByte = static_cast<uint8_t>((currentByte << 4) | nibble);
        nibbleCount++;
        if (nibbleCount == 2) {
            if (byteIndex >= 6) {
                return false;
            }
            parsed[byteIndex++] = currentByte;
            currentByte = 0;
            nibbleCount = 0;
        }
    }

    if (byteIndex != 6 || nibbleCount != 0) {
        return false;
    }

    memcpy(out, parsed, sizeof(parsed));
    return true;
}

void parseLine(const char* line, ObdCredentials& out) {
    if (line[0] == '#') {
        return;
    }

    const char* equalsSign = strchr(line, '=');
    if (equalsSign == nullptr) {
        return;
    }

    char key[16];
    size_t keyLen = static_cast<size_t>(equalsSign - line);
    if (keyLen >= sizeof(key)) {
        return;
    }
    memcpy(key, line, keyLen);
    key[keyLen] = '\0';

    const char* valueStr = equalsSign + 1;
    if (valueStr[0] == '\0') {
        return; // Blank value: leave the constructor default in place.
    }

    if (strcmp(key, "mac") == 0) {
        out.hasMac = parseMacAddress(valueStr, out.mac);
        if (!out.hasMac) {
            Serial.printf("[OBD] Ignoring malformed mac= value in %s; connecting by id instead.\n",
                          config::kObdConfigFilePath);
        }
    } else if (strcmp(key, "id") == 0) {
        strncpy(out.id, valueStr, sizeof(out.id) - 1);
        out.id[sizeof(out.id) - 1] = '\0';
    } else if (strcmp(key, "password") == 0) {
        strncpy(out.password, valueStr, sizeof(out.password) - 1);
        out.password[sizeof(out.password) - 1] = '\0';
    }
}

} // namespace

bool loadObdCredentials(SdManager& sdManager, ObdCredentials& out) {
    out = ObdCredentials();

    if (!sdManager.isMounted() || !SD.exists(config::kObdConfigFilePath)) {
        Serial.printf("[OBD] %s not found (SD mounted: %s).\n", config::kObdConfigFilePath,
                      sdManager.isMounted() ? "yes" : "no");
        return false;
    }

    File file = SD.open(config::kObdConfigFilePath, FILE_READ);
    if (!file) {
        Serial.printf("[OBD] Failed to open %s.\n", config::kObdConfigFilePath);
        return false;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) {
            continue;
        }
        parseLine(line.c_str(), out);
    }
    file.close();

    if (!out.hasMac || out.id[0] == '\0' || out.password[0] == '\0') {
        Serial.printf("[OBD] %s is missing/invalid required field(s) (mac=%s id=%s password=%s).\n",
                      config::kObdConfigFilePath, out.hasMac ? "ok" : "MISSING", out.id[0] != '\0' ? "ok" : "MISSING",
                      out.password[0] != '\0' ? "ok" : "MISSING");
        return false;
    }

    Serial.printf("[OBD] Loaded adapter identity from %s: id=%s\n", config::kObdConfigFilePath, out.id);
    return true;
}
