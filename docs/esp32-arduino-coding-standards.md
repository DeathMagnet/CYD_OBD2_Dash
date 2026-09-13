# ESP32 Arduino Coding Standards

## Pragmatic Hobbyist Edition

Modern C++, clean structure, no nonsense. These standards apply to all firmware under `src/` for the CYD OBD-II Dashboard. They supplement the project design in [CYD OBD-II Dashboard Implementation Guide](cyd-obd2-dashboard-implementation.md).

The goal is reliable, understandable firmware on a memory-constrained ESP32. Prefer plain, direct code that can be debugged through the PlatformIO monitor over clever abstractions or framework-sized architecture.

## Baseline Rules

- Build, upload, and monitor with the PlatformIO extension in VS Code using the `cyd_4inch` environment. Use `cyd_4inch_sim` only for bench work without an ELM327 adapter; it must never be the environment that ships.
- Write C++ compatible with the Arduino framework and the ESP32 toolchain configured by `platformio.ini`.
- Keep application source under `src/`; organize by responsibility rather than by file type alone.
- Use UTF-8 source files, but keep identifiers, comments, logs, and messages in clear English.
- Use four spaces for indentation. Do not use tabs.
- Prefer lines under 100 characters when practical, but do not distort readable expressions solely to meet a width limit.
- Make each change build before moving to the next subsystem.

## File and Naming Conventions

Use lower snake case for source filenames, `PascalCase` for types, and `camelCase` for functions, methods, and variables.

```text
src/
  main.cpp
  app_config.h
  labels.h
  obd/
    obd_client.h
    obd_client.cpp
    telemetry.h
  display/
    dashboard_renderer.h
    dashboard_renderer.cpp
```

```cpp
class ObdClient {
public:
  bool begin();
  void update(uint32_t nowMs);

private:
  uint32_t nextPollAtMs_ = 0;
};

constexpr uint32_t kDisplayRefreshIntervalMs = 33;
```

Apply these naming rules consistently:

- Classes, structs, enums, and enum values: `PascalCase`.
- Functions and local variables: `camelCase`.
- Private members: `camelCase_` with a trailing underscore.
- Compile-time constants: `kPascalCase`.
- Include guards: `PROJECT_PATH_FILE_NAME_H` when `#pragma once` is unavailable.
- Boolean names: begin with `is`, `has`, `can`, `should`, `was`, or an unambiguous state word such as `connected`.

Use names that express units and ownership. Prefer `timeoutMs`, `speedKph`, `coolantC`, and `frameBuffer` to vague names such as `timeout`, `speed`, `temp`, or `data`.

## Headers and Dependencies

Use `#pragma once` in project headers. Headers expose the smallest stable interface needed by another module; implementation detail belongs in `.cpp` files.

```cpp
#pragma once

#include <stdint.h>

struct TelemetrySnapshot;

class DashboardRenderer {
public:
  bool begin();
  void render(const TelemetrySnapshot& telemetry, uint32_t nowMs);
};
```

- Include the matching header first in every `.cpp` file, followed by standard-library headers, Arduino/framework headers, then project headers.
- Include what the file directly uses; do not rely on transitive includes.
- Forward-declare types in headers when a pointer or reference is sufficient.
- Avoid global `using namespace ...` directives in headers and source files.
- Add libraries only through `lib_deps` in `platformio.ini`, with a pinned or compatible version. Verify that a dependency supports the ESP32 Arduino framework before adding it.
- Pull `TFT_eSPI` from its GitHub source (`https://github.com/Bodmer/TFT_eSPI.git`) rather than a versioned registry entry. The PlatformIO registry mirror has lagged behind the version Arduino Library Manager serves, and the two have shipped with different `TFT_eSPI.cpp`/`TFT_eSPI.h`/ESP32 processor backends despite similar version numbers; a stale registry copy is a real source of hard-to-diagnose display/touch behavior differences from what was verified in Arduino IDE.

## Modern C++ That Fits Arduino

Use modern C++ features that improve correctness without making the firmware opaque.

- Prefer `enum class` over unscoped enums.
- Prefer `constexpr` for constants known at compile time.
- Use constructors and in-class member initialization for valid defaults.
- Pass read-only objects by `const&` when copying is non-trivial; pass small scalars by value.
- Mark functions `const` when they do not change observable object state.
- Use `nullptr`, never `NULL` or integer zero for pointers.
- Prefer fixed-width integer types such as `uint32_t` and `int16_t` for protocol, timing, storage, and hardware values.
- Prefer `size_t` for buffer lengths and indexes derived from container sizes.
- Avoid exceptions, RTTI, `dynamic_cast`, and heap-heavy abstractions in normal firmware paths.
- Do not use `new`, `delete`, or unbounded `String` concatenation in recurring code paths.

`String` is acceptable for small, infrequent setup-time or diagnostic formatting. For display, Bluetooth, logging, and loop-time work, use fixed buffers, bounded formatting, or preallocated objects.

```cpp
enum class ConnectionState : uint8_t {
  Connecting,
  Live,
  Stale,
  Fault,
};

constexpr uint32_t kReconnectDelayMs = 2'000;

bool isDue(uint32_t nowMs, uint32_t dueAtMs) {
  return static_cast<int32_t>(nowMs - dueAtMs) >= 0;
}
```

The signed subtraction in `isDue` preserves correct scheduling when the `millis()` counter wraps around.

## Application Structure and State

Keep `setup()` and `loop()` small. `setup()` initializes dependencies in order; `loop()` reads the clock once and advances independent subsystems without blocking.

```cpp
void loop() {
  const uint32_t nowMs = millis();

  obdClient.update(nowMs);
  bootPlayer.update(nowMs);
  csvLogger.update(telemetry, nowMs);
  dashboardRenderer.render(telemetry, nowMs);
}
```

- Each module owns one responsibility: OBD transport/polling, rendering, boot playback, or CSV persistence.
- Exchange data through explicit types such as `TelemetrySnapshot`, not direct access to another module's internal state.
- Model connection and feature status with `enum class`, not scattered boolean combinations.
- Centralize user-adjustable values in `app_config.h`; do not duplicate display dimensions, thresholds, intervals, pins, or units across modules.
- Centralize on-screen UI text (gauge labels, units, button/status strings, page titles) in `src/labels.h` (`namespace labels`) rather than as literals inside `.cpp` files.
- Do not modify pins, drivers, feature flags, partition tables, or library versions outside `platformio.ini` without documenting why.

## Non-Blocking Firmware

The display, OBD connection, boot animation, and CSV logging must remain responsive together. Do not put long work in `loop()`.

- Never use long `delay()` calls in normal operation.
- Schedule periodic work with `millis()` and per-task deadlines.
- Perform one bounded unit of work per update: one OBD request, one boot frame, one log row, or one display region.
- Use reconnect backoff and timeouts rather than retry loops that monopolize the CPU.
- Keep rendering separate from Bluetooth reads and file writes.
- Check return values from display, Bluetooth, SD, and file operations; change state or report a controlled fault on failure.

Avoid this:

```cpp
while (!obdConnected()) {
  connectObd();
  delay(5000);
}
```

Use a scheduled state transition instead:

```cpp
if (connectionState_ == ConnectionState::Connecting && isDue(nowMs, nextConnectAtMs_)) {
  if (tryConnect()) {
    connectionState_ = ConnectionState::Live;
  } else {
    nextConnectAtMs_ = nowMs + kReconnectDelayMs;
  }
}
```

## Data Validation and Error Handling

Treat Bluetooth responses, OBD-II PID values, SD-card availability, and boot assets as untrusted external input.

- Validate response length, format, numeric ranges, and checks before publishing a value.
- Represent unavailable data explicitly with `valid = false`; do not fabricate a zero reading.
- Record the last-update time per telemetry field and render stale values differently from live values.
- Return `bool` for operations with a simple success/failure result. Return a small `enum class` when callers need to distinguish failure reasons.
- Keep errors local when possible: SD failure disables logging; it must not stop gauge rendering or OBD recovery.
- Avoid `assert` as a runtime recovery strategy. Use it only for developer invariants where the toolchain supports it.
- Never expose pairing PINs, Bluetooth addresses, or other user-specific settings in committed source or serial logs.

```cpp
struct TelemetryValue {
  float value = 0.0F;
  bool valid = false;
  uint32_t updatedAtMs = 0;
};

bool parseRpmResponse(const char* response, TelemetryValue& rpm, uint32_t nowMs) {
  float parsedRpm = 0.0F;
  if (!decodeRpm(response, parsedRpm) || parsedRpm < 0.0F || parsedRpm > 12'000.0F) {
    return false;
  }

  rpm = {parsedRpm, true, nowMs};
  return true;
}
```

## Memory, Buffers, and Performance

The ESP32 has more room than an 8-bit Arduino, but the display, Bluetooth stack, filesystem, and animation assets contend for RAM. Make allocation deliberate.

- Prefer stack allocation for small, short-lived objects and statically sized buffers for known protocol limits.
- Preallocate sprites and reusable buffers during initialization; do not create/destroy them every frame.
- Check allocation results for large display sprites or buffers and provide a reduced-detail fallback when allocation fails.
- Store immutable asset data in flash using the project/toolchain-supported mechanism.
- Avoid copying full telemetry snapshots or image frames when a `const&` works.
- Redraw changed display regions or use sprites to prevent flicker and avoid unnecessary SPI traffic.
- Open CSV session files once, write at a bounded interval, and flush intentionally rather than on every frame.
- Measure timing through serial diagnostics during development before attempting optimizations.

## Display, Logging, and OBD-Specific Rules

- Use the configured `TFT_eSPI` ST7796S pins and frequency from `platformio.ini`; do not hardcode duplicate pin values in source.
- Set display rotation explicitly and verify the logical canvas is 480 x 320 before drawing dashboard UI or boot assets.
- Format display and CSV numeric output with explicit units and deterministic decimal precision.
- Keep CSV fields stable and leave unavailable readings empty.
- Keep OBD polling priorities explicit: speed and RPM may update more often than secondary PIDs.
- Do not request PIDs faster than the adapter can reliably answer. Use measurements from the monitor to set intervals.
- Guard optional functionality with the applicable build flags, including `SD_LOGGING_ENABLED` and `BOOT_RGB666_ASSETS_AVAILABLE`.
- Provide a visible status for connecting, live, stale, OBD unavailable, and SD offline states.
- Keep the TFT/touch SPI bus on HSPI (`USE_HSPI_PORT` in `platformio.ini`) and the SD card on its own VSPI instance (`SdManager`). Classic ESP32 exposes one active MISO input source per SPI peripheral; if both subsystems default to the same peripheral, whichever `begin()` runs later silently reassigns the shared read line, breaking touch input while display writes keep working normally (writes fan out to all attached pins, reads do not). Do not remove `USE_HSPI_PORT` without re-verifying touch after any SD-path change.
- When regenerating a boot/splash image array (e.g. `boot_0_rgb565.h`), verify the generated pixels against the source image (e.g. compare average per-channel values) rather than trusting the conversion tool — a truncated or mis-packed pixel format will compile and run but render with an incorrect tint. `pushImage()` also requires `setSwapBytes(true)` on this panel for a standard (non-pre-swapped) RGB565 array; `fillScreen`/`fillRect` do not need it because they swap bytes internally.

## Logging and Diagnostics

Serial logs are for diagnosis, not a second UI.

- Initialize `Serial` at the `115200` baud rate configured in `platformio.ini`.
- Log state transitions, recoverable failures, PID support decisions, and initialization failures.
- Do not emit per-frame logs or full raw payloads continuously; rate-limit noisy diagnostics.
- Use consistent, searchable prefixes such as `[OBD]`, `[SD]`, `[DISPLAY]`, and `[BOOT]`.
- Remove temporary debug output before merging unless it has clear operational value.

```cpp
Serial.printf("[OBD] Connection failed; retrying in %lu ms\n",
              static_cast<unsigned long>(kReconnectDelayMs));
```

## Comments and Documentation

Write comments for intent, trade-offs, hardware constraints, and non-obvious timing behavior. Do not narrate syntax.

```cpp
// Keep SD writes off the render cadence to avoid visible gauge stalls.
if (isDue(nowMs, nextLogAtMs_)) {
  writeTelemetryRow(snapshot);
}
```

Update [CYD OBD-II Dashboard Implementation Guide](cyd-obd2-dashboard-implementation.md) when a feature changes its documented architecture, hardware contract, external dependency, configuration, or verification process.

## Pre-Upload Checklist

Before uploading firmware:

- Confirm the PlatformIO environment is `cyd_4inch` (or `cyd_4inch_sim` when bench-testing without an adapter).
- Build with the PlatformIO VS Code extension and resolve all compiler errors.
- Use **Clean** then **Build** after any `platformio.ini`, dependency, partition, or compile-flag change.
- Test the edited behavior through the PlatformIO monitor at `115200` baud.
- Verify that disconnects, missing PIDs, missing SD cards, and invalid boot assets preserve a responsive dashboard.
- Confirm no secrets, diagnostic floods, placeholder implementations, unused includes, or dead code were introduced.
- Update the implementation guide when the change affects its stated contract.

## References

- [CYD OBD-II Dashboard Implementation Guide](cyd-obd2-dashboard-implementation.md)
- [PlatformIO configuration](../platformio.ini)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [Espressif Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/)