#pragma once

#include <stdint.h>

class SdManager;

// ELM327 Bluetooth identity, loaded once at boot from /obd_config.txt on the
// SD card (see obd_credentials.cpp) rather than a live/UI-editable setting -
// mac, id, and password are all mandatory fields in that file, with no
// fallback defaults. There is still no full on-device settings page for
// these values, but a missing/invalid file, or stored credentials that fail
// to connect, now falls back to a boot-time Bluetooth pairing/recovery
// screen (see src/obd/obd_pairing.h) that can write a fresh file itself
// rather than requiring the SD card to be hand-edited.
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

// Writes mac=/id=/password= to /obd_config.txt, overwriting any existing
// file (mirrors ConfigStore::save()'s remove-then-FILE_WRITE pattern).
// Called by the pairing screen (src/obd/obd_pairing.cpp) every time the user
// taps Connect. Returns false if the SD card isn't mounted or the file can't
// be opened for writing.
bool saveObdCredentials(SdManager& sdManager, const ObdCredentials& creds);
