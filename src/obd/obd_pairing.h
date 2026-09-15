#pragma once

class SdManager;
class TouchManager;
class DisplayManager;
struct ThemeColors;
struct ObdCredentials;

// Boot-time-only Bluetooth pairing/recovery UI for the OBD-II adapter
// identity normally read from /obd_config.txt (see obd/obd_credentials.h).
// Both functions here run entirely inside main.cpp's setup(), before
// ObdClient::begin() is ever called, using their own temporary
// BluetoothSerial instance so ObdClient's runtime connect/retry/backoff
// logic (obd_client.cpp) is never touched. Neither is invoked once the
// dashboard is showing - a dropped connection there keeps using ObdClient's
// existing infinite background retry instead, exactly as before this
// feature existed.
namespace obd_pairing {

// Tries connecting with `creds`' stored mac/password up to
// config::kPreflightMaxAttempts times, using a temporary local
// BluetoothSerial. Returns true on success (the test connection has already
// been disconnected and the BluetoothSerial torn down before returning).
// Called once, right after a valid /obd_config.txt loads, to decide whether
// run() is needed.
bool preflight(const ObdCredentials& creds);

// Blocks until a Bluetooth connection to an ELM327 adapter succeeds - there
// is no cancel path, matching the recovery-flow spec. Scans for nearby
// Bluetooth devices (filtered to names matching known OBD-adapter patterns,
// falling back to the full list if none match), draws a 3-button screen
// (Device / Password / Connect) and lets the user cycle Device/Password and
// tap Connect, writing /obd_config.txt via saveObdCredentials() on every
// Connect tap. Five failed Connect attempts reset the selection and
// re-scan. On success, `out` holds the just-verified credentials and this
// function's BluetoothSerial has already been torn down (end()) so
// ObdClient::begin() gets a clean Bluetooth stack.
void run(DisplayManager& displayManager, TouchManager& touchManager, SdManager& sdManager,
         const ThemeColors& theme, ObdCredentials& out);

} // namespace obd_pairing
