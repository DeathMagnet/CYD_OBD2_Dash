#pragma once

// Machine-specific ELM327 adapter identity.
//
// Copy this file to "local_config.h" (same directory) and adjust the values
// for your own adapter. "local_config.h" is gitignored so personal Bluetooth
// identifiers never land in version control. If "local_config.h" is absent,
// obd_client.cpp falls back to the generic defaults in app_config.h.

namespace localconfig {

// Bluetooth SPP device name advertised by most generic ELM327 v1.5 clones.
// Change this if your adapter advertises a different name (check your phone's
// Bluetooth pairing list before flashing).
constexpr const char* kObdAdapterName = "OBDII";

// Most ELM327 clones use a fixed legacy PIN. Common values are "1234" or "0000".
constexpr const char* kObdAdapterPin = "1234";

} // namespace localconfig
