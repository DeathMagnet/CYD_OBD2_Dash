#pragma once

#include <stdint.h>

class SdManager;

// ELM327 Bluetooth identity, loaded once at boot from /obd_config.txt on the
// SD card (see obd_credentials.cpp) rather than a live/UI-editable setting -
// there's no on-device config page for this anymore, and no fallback
// defaults: mac, id, and password are all mandatory fields in that file.
struct ObdCredentials {
    bool hasMac = false;
    uint8_t mac[6] = {0};
    char id[24] = {0};
    char password[9] = {0};
};

// Reads /obd_config.txt (config::kObdConfigFilePath) and populates `out`.
// Returns true only if the SD card is mounted, the file exists and opens,
// and mac/id/password are all present and valid (mac must parse as 6 hex
// bytes; id/password must be non-empty) - there is no fallback. Returns
// false otherwise, in which case the caller must treat this as fatal (the
// contents of `out` are undefined on failure).
bool loadObdCredentials(SdManager& sdManager, ObdCredentials& out);
