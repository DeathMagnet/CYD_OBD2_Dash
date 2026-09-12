# CYD OBD-II Dashboard Implementation Guide

## Purpose

Implement a real-time automotive dashboard for the Hosyond 4.0-inch ESP32-32E CYD. The device reads OBD-II telemetry through a Bluetooth ELM327 adapter, presents it on the ST7796S display in landscape orientation, optionally plays a boot animation, and stores structured telemetry logs on an SD card.

This guide describes the initial implementation boundary. The repository currently has no application source tree, so create the application under `src/` while preserving the PlatformIO hardware configuration in `platformio.ini`.

## Key Features

- 📊 **Live OBD-II gauges** — RPM, speed, coolant temp, throttle position, battery voltage, and more
- 🎨 **Multi-theme UI** — Mustang OEM S197, Torque, and Modern Flat themes, switchable at runtime
- 🐴 **Boot animation** — Optional frame-by-frame RGB666 pony sprite sequence on startup
- 💾 **SD card logging** — Timestamped CSV telemetry logs written continuously while driving
- 🔔 **Warning overlays** — On-screen alerts for high temp, low voltage, and check-engine conditions
- 📡 **Bluetooth OBD-II** — Wireless connection to any ELM327 v1.5+ adapter
- ⚡ **Fast rendering** — Double-buffered TFT_eSPI drawing for flicker-free gauges

## Product Requirements

The dashboard must:

- Operate on the native 320 x 480 TFT in 480 x 320 landscape orientation.
- Connect to an ELM327-compatible OBD-II adapter over Bluetooth.
- Display responsive gauges and live telemetry suitable for a driving HUD.
- Support both a static boot image and an animated boot sequence.
- Write telemetry as recoverable, structured CSV files on the SD card.
- Continue running safely when the adapter disconnects, a PID is unavailable, or the SD card is missing.
- Avoid blocking the display update loop during Bluetooth reads, SD writes, or animation playback.

The target experience is a readable Mustang-themed dashboard, but styling must never reduce legibility or conceal fault/disconnect states.

## Hardware and Build Contract

Use the existing `env:cyd_4inch` environment. The current configuration is authoritative unless the hardware is physically changed.

| Component | Required configuration |
| --- | --- |
| Framework | Arduino on Espressif32 |
| Board | `esp32dev` |
| Display controller | ST7796S via TFT_eSPI (HSPI) |
| Physical panel | 320 x 480 pixels |
| Render orientation | 480 x 320 landscape |
| TFT SPI pins | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2 |
| Display reset | Not connected (`TFT_RST=-1`) |
| Backlight | GPIO 27 |
| Touch chip select | GPIO 33 |
| SD Card SPI pins | CS 5, MOSI 23, MISO 19, CLK 18 (VSPI) |
| TFT SPI frequency | 20 MHz |
| SD logging switch | `SD_LOGGING_ENABLED` build flag |
| Boot image mode | `BOOT_IMAGE_MODE` build flag |
| RGB666 assets | `BOOT_RGB666_ASSETS_AVAILABLE` build flag |

Do not repurpose TFT, touch, or SD SPI pins without verifying the board schematic and the relevant TFT_eSPI setup. Initialize the display before drawing any boot asset and explicitly select the landscape rotation that yields a width of 480 and height of 320. Fail initialization visibly when these dimensions are not available.

## VS Code and PlatformIO Workflow

Build, flash, and monitor this project through the PlatformIO extension in Visual Studio Code. Treat `platformio.ini` as the single source of truth for the active environment; do not create separate Arduino IDE configuration or manually duplicate build flags in VS Code tasks.

### Required Developer Setup

1. Install Visual Studio Code and the official PlatformIO IDE extension.
2. Open the repository root (`CYD_OBD2_Dash`) as the VS Code workspace folder. Do not open `src/` or an individual source file as the workspace.
3. Allow PlatformIO to finish installing the Espressif32 platform, Arduino framework, and the dependencies declared in `platformio.ini`.
4. In the PlatformIO environment selector, choose `cyd_4inch`. All project actions must target this environment.
5. Connect the ESP32 over a data-capable USB cable and select its serial device under **PlatformIO: Serial Port** when automatic detection does not choose the correct port.

### Daily Build, Upload, and Monitor Cycle

Use the PlatformIO toolbar, the PlatformIO sidebar, or the Command Palette for the following actions:

| Activity | PlatformIO action | Expected result |
| --- | --- | --- |
| Resolve dependencies | `PlatformIO: Rebuild Project Index` | IntelliSense and project metadata match `platformio.ini` |
| Compile | **Project Tasks > cyd_4inch > General > Build** | Firmware compiles for the ESP32 target |
| Upload | **Project Tasks > cyd_4inch > General > Upload** | Firmware is flashed to the selected serial device |
| Build then upload | **Project Tasks > cyd_4inch > General > Upload and Monitor** | Firmware is flashed and the serial monitor opens |
| Inspect runtime logs | **Project Tasks > cyd_4inch > General > Monitor** | Serial output opens at `115200` baud |
| Erase a development device | **Project Tasks > cyd_4inch > Platform > Erase Flash** | Flash is cleared before a known-clean upload |

The terminal equivalent, `pio run -e cyd_4inch`, may be used to diagnose a CI or extension failure, but the intended development and deployment workflow remains the PlatformIO VS Code extension.

### Extension Troubleshooting Rules

- Confirm the environment selector still reads `cyd_4inch` before diagnosing a failed build or upload.
- Use **Clean** followed by **Build** after changing `platformio.ini`, compiler flags, partitions, or library versions; this avoids stale `.pio` artifacts.
- Verify that the selected serial port is the ESP32 device before uploading. Disconnect other serial devices when port selection is ambiguous.
- Close any other serial terminal before opening the PlatformIO monitor, because only one process can normally own the port.
- Keep monitor output at the configured `115200` baud rate. Treat unreadable output as a port/baud configuration issue before debugging firmware logic.
- Record the complete PlatformIO task output when reporting a build, upload, or monitor failure; include the active environment and selected serial port.

## Proposed Source Layout

Create the following focused modules. Keep `setup()` limited to initialization and `loop()` limited to scheduling non-blocking work.

```text
src/
  main.cpp
  app_config.h
  display/
    dashboard_renderer.h
    dashboard_renderer.cpp
    boot_player.h
    boot_player.cpp
    theme.h
  obd/
    obd_client.h
    obd_client.cpp
    telemetry.h
  logging/
    csv_logger.h
    csv_logger.cpp
  system/
    connection_state.h
    scheduler.h
```

The exact filenames may follow local conventions once source exists, but preserve these responsibilities:

- `main.cpp`: Own subsystem instances, initialize in dependency order, and call periodic update methods.
- `telemetry`: Define one typed snapshot representing the latest readings and validity/freshness metadata.
- `obd_client`: Own Bluetooth/ELM327 connection state, PID polling, parsing, and reconnect policy.
- `dashboard_renderer`: Render gauges, telemetry, warnings, boot-to-dashboard transition, and a stale-data indication.
- `boot_player`: Decode and schedule static or animated assets without blocking normal startup indefinitely.
- `csv_logger`: Mount the SD card, create session files, write header and rows, batch/flush writes, and expose errors.
- `connection_state`: Express startup, connecting, live, stale, and fault states shared by the renderer and OBD client.

## Application Lifecycle

Implement the lifecycle as an explicit state machine rather than a sequence of delays:

```text
Boot -> DisplayReady -> BootAsset -> SdInit -> ObdConnecting -> Live
                                      |              |             |
                                      v              v             v
                                   Degraded <---- Reconnecting <- Stale
```

1. Initialize serial output at the monitor speed configured by PlatformIO (`115200`).
2. Initialize the display, set landscape rotation, clear the screen, and draw a minimal status screen.
3. Play the selected boot asset. Enforce a finite duration; the user must reach a connecting/status screen even if an animation asset is corrupt or absent.
4. Attempt to mount the SD card only when `SD_LOGGING_ENABLED` is defined. A failed mount must disable logging while retaining all dashboard functionality.
5. Start Bluetooth and attempt the configured ELM327 connection.
6. Poll supported PIDs on a schedule, publish complete telemetry snapshots, and enter `Live` only after valid engine data arrives.
7. On timeout or disconnect, retain the last value only as stale data, render the stale/disconnected state clearly, and retry with bounded backoff.

Do not use long `delay()` calls for connection retries, boot frames, PID polling, gauge animation, or SD writes. Schedule work using `millis()` and independently track each subsystem's next due time.

## OBD-II Integration

Use an ELM327-compatible Bluetooth client/library appropriate for the ESP32 Arduino framework. Verify the selected library's API, dependency version, Bluetooth transport support, and license before adding it to `lib_deps`.

Begin with a small, useful PID set:

| Field | OBD-II PID | Unit | Dashboard role |
| --- | --- | --- | --- |
| Engine RPM | `01 0C` | rpm | Primary tachometer |
| Vehicle speed | `01 0D` | km/h or mph | Primary speed gauge |
| Coolant temperature | `01 05` | C or F | Warning-capable auxiliary value |
| Engine load | `01 04` | percent | Auxiliary value |
| Intake air temperature | `01 0F` | C or F | Auxiliary value |
| Throttle position | `01 11` | percent | Auxiliary value |

Store internally normalized units, then apply display-unit preferences at render time. Do not assume every ECU supports every PID. Each field requires a validity flag and last-updated timestamp so an unsupported or timed-out PID cannot be displayed as a genuine zero.

A suitable snapshot shape is:

```cpp
struct TelemetryValue {
  float value;
  bool valid;
  uint32_t updatedAtMs;
};

struct TelemetrySnapshot {
  TelemetryValue rpm;
  TelemetryValue speedKph;
  TelemetryValue coolantC;
  TelemetryValue engineLoadPercent;
  TelemetryValue intakeAirC;
  TelemetryValue throttlePercent;
  bool connected;
  uint32_t capturedAtMs;
};
```

Treat the snapshot as the renderer and logger boundary. The OBD client updates it only after parsing a response; the renderer must not perform Bluetooth I/O. Poll high-priority values such as RPM and speed more frequently than secondary PIDs, while avoiding adapter overload. Start with measured refresh behavior and tune using serial timing data rather than hard-coded optimistic intervals.

## Display and Gauge Behavior

Render from the latest `TelemetrySnapshot` at a stable cadence. Gauge animation should interpolate from the previously rendered value to the newest valid value, but the data source remains the actual latest reading.

Implement these presentation rules:

- Use `TFT_eSPI` primitives, sprites, and text rendering appropriate to the device's memory budget.
- Keep the primary speed and RPM readable at a glance, including at minimum and maximum values.
- Redraw only changed regions or use sprites to avoid visible flicker.
- Use a distinct status strip or badge for `CONNECTING`, `LIVE`, `STALE`, `NO OBD`, and `SD OFFLINE` states.
- Mark fields with missing/invalid data using a neutral placeholder such as `--`; never substitute zero.
- Define warning thresholds in `app_config.h`, separate from rendering code.
- Use color as a secondary cue; warnings must remain understandable through labels, placement, and contrast.
- Ensure screen updates stay responsive while logging and reconnecting.

Keep layout dimensions, colors, gauge ranges, units, polling intervals, animation duration, and feature switches centralized in configuration rather than duplicated across rendering files.

## Boot Assets

Support static and animated boot assets selected at compile time through `BOOT_IMAGE_MODE`. Use `BOOT_RGB666_ASSETS_AVAILABLE` to guard asset-dependent code so a build without generated assets still compiles and reaches the dashboard.

For RGB666 data:

- Store generated assets in a clearly named source or include directory and document the generator/source format.
- Validate width, height, byte count, and frame count before drawing.
- Render only assets matching the configured orientation, or rotate/convert them during asset generation rather than at runtime.
- Limit per-frame work so animation timing does not starve Bluetooth or the watchdog.
- Provide a simple text fallback boot screen when the selected asset is unavailable or invalid.

Do not embed unverified image data or allocate a full uncompressed multi-frame animation in RAM. Store static data in flash and stream frames when asset size requires it.

## SD Card CSV Logging

`SD_LOGGING_ENABLED` controls whether logging is compiled in. The firmware must compile and run without the flag, and it must continue when an enabled build has no usable card.

Create one session file per boot using a collision-resistant name, such as a monotonic boot/session number when no real-time clock is available. Write the CSV header exactly once for each new file. Use a stable schema with ISO-8601 time only when an actual time source is available; otherwise log device uptime explicitly.

Recommended schema:

```csv
uptime_ms,connected,rpm,speed_kph,coolant_c,engine_load_percent,intake_air_c,throttle_percent
0,0,,,,,,,
1542,1,820,0,77.0,18.8,31.0,12.2
```

Logging rules:

- Write a row at a configurable interval, independent of display frame rate.
- Preserve missing readings as empty CSV fields rather than `0` or fabricated values.
- Sanitize/format numeric values deterministically; do not write locale-specific decimal separators.
- Check file-open and write results and enter an `SD OFFLINE` state on failure.
- Buffer modestly and flush periodically to balance crash resilience with flash/card wear and frame latency.
- Avoid opening and closing the file for every telemetry row when a sustained session file can be kept safely open.
- Close and flush the file during controlled shutdown/restart paths where supported.

## Configuration and Secrets

Place user-adjustable values in `app_config.h`, including the Bluetooth adapter identity or pairing configuration, units, gauge ranges, warning thresholds, display/log intervals, and boot mode defaults. Do not hardcode personal adapter addresses, PINs, Wi-Fi credentials, or tokens in tracked source. Provide an ignored local configuration header or documented build flags for sensitive machine-specific settings.

## Implementation Sequence

1. Create the source scaffold and a minimal `setup()`/`loop()` that initializes serial and the TFT in verified landscape orientation.
2. Implement the status renderer and static mock telemetry. Confirm the UI does not flicker and dimensions are 480 x 320.
3. Add the telemetry snapshot model and OBD connection state machine, initially logging response timing to serial.
4. Add PID polling and update the UI from snapshots. Test disconnect, reconnect, unsupported PID, and timeout behavior.
5. Implement SD mounting and CSV session logging behind `SD_LOGGING_ENABLED`; test with a mounted card and without a card.
6. Add static boot assets, then animated playback with asset validation and fallback behavior.
7. Replace temporary diagnostics with intentional status/warning surfaces, retain only useful production logging, and document configuration options.

## Verification Checklist

Run these checks as implementation progresses:

- In the PlatformIO extension, run **Build** for `cyd_4inch` with the default feature flags.
- Temporarily remove `SD_LOGGING_ENABLED`, use **Clean** and then **Build** for `cyd_4inch`, and confirm logging is cleanly optional. Restore the flag after the check.
- Use **Upload and Monitor** for `cyd_4inch`, verify the ESP32 upload completes, and confirm runtime logs are readable at `115200` baud.
- Flash the board and verify the display reports 480 x 320 after selecting landscape rotation.
- Boot with valid and missing boot assets; both paths must reach the connection screen.
- Connect to a known ELM327 adapter and confirm RPM, speed, and at least one secondary PID update.
- Turn off the adapter or leave vehicle range; verify that the UI becomes stale/disconnected without freezing or reporting false zero values.
- Test an ECU/PID failure and verify only the affected field is invalid.
- Insert an SD card, confirm a new CSV has one header and parseable rows, then inspect it on a host machine.
- Repeat without an SD card and confirm the dashboard stays functional while reporting logging unavailable.
- Observe display rendering and Bluetooth recovery for an extended bench session; confirm no uncontrolled memory growth, watchdog resets, or UI stalls.

## Safety and Scope Notes

This is an informational dashboard, not a vehicle control system. It must not transmit commands that modify vehicle behavior, distract from driving, or encourage interaction while moving. Keep vehicle-specific assumptions configurable and test only in a safe, stationary environment before road use.

## References

- [CYD OBD-II UI Cluster Guide](cyd-obd2-ui-cluster-guide.md)
- [CYD OBD-II SD Card Telemetry Logging Guide](cyd-obd2-sd-logging-guide.md)
- [ESP32 Arduino Coding Standards](esp32-arduino-coding-standards.md)
- [PlatformIO configuration](../platformio.ini)
- [Project overview](../README.md)
- [TFT_eSPI documentation](https://github.com/Bodmer/TFT_eSPI)
- [PlatformIO ESP32 documentation](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [akos-sereg/esp32-obd2](https://github.com/akos-sereg/esp32-obd2)
- [LaXiS96/LaX](https://github.com/LaXiS96/esp32-obd2)
- [wilson3682/CYD-4.0-320x480-Touch-Screen](https://github.com/wilson3682/CYD-4.0-320x480-Touch-Screen)
- [www.howtogeek.com](https://www.howtogeek.com/esp32-in-car-knows-things-my-dashboard-wont-tell-me/)
- [Torque Android App](https://torque-bhp.com/)
- [Wiki for Torque Android App](https://wiki.torque-bhp.com/view/Main_Page)
- [Torque Pro App](https://play.google.com/store/apps/details?id=org.prowl.torque&hl=en-US&pli=1)

## Hardware Used
- [4" 320x480 CYD](https://www.lcdwiki.com/index.php?title=4.0inch_ESP32-32E_Display)
- [ELM 327 OBD2 Scanner](https://www.amazon.com/dp/B0CCYKZ8YF?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)