# CYD OBD-II Dashboard Implementation Guide

## Purpose

Implement a real-time automotive dashboard for the Hosyond 4.0-inch ESP32-32E CYD. The device reads OBD-II telemetry through a Bluetooth ELM327 adapter, presents it on the ST7796S display in landscape orientation, optionally plays a boot animation, and stores structured telemetry logs on an SD card.

This guide describes the initial implementation boundary. The repository currently has no application source tree, so create the application under `src/` while preserving the PlatformIO hardware configuration in `platformio.ini`.

## Key Features

- 📊 **Live OBD-II gauges** — RPM, speed, coolant temp, throttle position, battery voltage, and more
- 🎨 **Multi-theme UI** — S197 - Digital, S197 - Analog, Neon, and Modern Flat themes, switchable at runtime
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

Use the existing `env:cyd_4inch` environment. The current configuration is authoritative unless the hardware is physically changed. `env:cyd_4inch_sim` inherits everything from it and only adds `OBD_SIMULATION_ENABLED`; see "Simulated Telemetry Build" below.

| Component | Required configuration |
| --- | --- |
| Framework | Arduino on Espressif32 |
| Board | `esp32dev` |
| Display controller | ST7796S via TFT_eSPI (HSPI) |
| Physical panel | 320 x 480 pixels |
| Render orientation | 480 x 320 landscape, runtime-flippable 180° via the Config: UI page's Flip Screen setting (persisted to SD, applied on the restart Save triggers) |
| TFT SPI pins | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2 |
| Display reset | Not connected (`TFT_RST=-1`) |
| Backlight | GPIO 27 |
| Touch chip select | GPIO 33 |
| SD Card SPI pins | CS 5, MOSI 23, MISO 19, CLK 18 (VSPI) |
| TFT SPI frequency | 20 MHz |
| SD logging switch | `SD_LOGGING_ENABLED` build flag |
| Simulated telemetry switch | `OBD_SIMULATION_ENABLED` build flag (set by `env:cyd_4inch_sim`) |
| Boot image mode | `BOOT_IMAGE_MODE` build flag |
| RGB666 assets | `BOOT_RGB666_ASSETS_AVAILABLE` build flag |

> **Note on `BOOT_IMAGE_MODE` and `BOOT_RGB666_ASSETS_AVAILABLE`:** Both flags are currently defined in `platformio.ini` but not yet read or acted upon by any source code. The static PNG boot logo (`src/assets/boot_logo_png.h`) is used unconditionally regardless of these flags; they are placeholders for future implementation of the animated RGB666 frame format and alternate boot modes. Code branching on either flag will have no effect at present.

Do not repurpose TFT, touch, or SD SPI pins without verifying the board schematic and the relevant TFT_eSPI setup. Initialize the display before drawing any boot asset and explicitly select the landscape rotation that yields a width of 480 and height of 320. Fail initialization visibly when these dimensions are not available.

## VS Code and PlatformIO Workflow

Build, flash, and monitor this project through the PlatformIO extension in Visual Studio Code. Treat `platformio.ini` as the single source of truth for the active environment; do not create separate Arduino IDE configuration or manually duplicate build flags in VS Code tasks.

### Required Developer Setup

1. Install Visual Studio Code and the official PlatformIO IDE extension.
2. Open the repository root (`CYD_OBD2_Dash`) as the VS Code workspace folder. Do not open `src/` or an individual source file as the workspace.
3. Allow PlatformIO to finish installing the Espressif32 platform, Arduino framework, and the dependencies declared in `platformio.ini`.
4. In the PlatformIO environment selector, choose `cyd_4inch`. All project actions must target this environment, except bench sessions that deliberately select `cyd_4inch_sim`.
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

The terminal equivalent, `pio run -e cyd_4inch`, may be used to diagnose a CI or extension failure, but the intended development and deployment workflow remains the PlatformIO VS Code extension. Substitute `cyd_4inch_sim` in any of the above to run the simulated build; the task names are otherwise identical.

### Extension Troubleshooting Rules

- Confirm the environment selector still reads `cyd_4inch` before diagnosing a failed build or upload. A device showing plausible telemetry with no adapter paired is running a `cyd_4inch_sim` build.
- After editing `platformio.ini` section structure, verify the *resolved* flags with `pio project config --json-output` rather than reading the file. Moving settings between sections can silently drop lines from a multi-line `build_flags` value, and losing the `LOAD_FONT*` flags makes every `drawString()` call render nothing while graphics still appear.
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

### As-Built Source Layout

The tree above was the starting proposal; the cluster UI and OBD/logging work landed with a more granular split (each `.h` has a matching `.cpp` except where noted):

```text
src/
  main.cpp
  app_config.h
  labels.h                   # Centralized UI label/status/button strings (namespace labels)
  display/
    display_manager.h/.cpp   # TFT init, backlight, boot image (pre-existing)
    theme.h/.cpp             # ThemeColors + getTheme() - S197 - Digital, S197 - Analog, Neon, and Modern Flat all implemented
    gauge_widgets.h/.cpp     # Arc gauge, bar gauge, value box, status badge, MIL indicator, NeedlePhysics
    cluster_layout.h         # Shared pixel geometry (header only) used by both drawing and touch hit-testing
    cluster_pages.h/.cpp     # Dashboard pages and config pages drawStatic()/drawDynamic()
    touch_handler.h/.cpp     # ClusterTouchHandler: nav zones, config page taps, group cycling
  input/
    touch_manager.h/.cpp     # Raw touch + calibration (pre-existing)
  obd/
    telemetry.h              # TelemetryValue / TelemetrySnapshot (header only)
    obd_pids.h/.cpp          # PID table, decode formulas, response-line parser
    obd_credentials.h/.cpp   # Loads/saves mac/id/password from /obd_config.txt on SD at boot
    obd_client.h/.cpp        # ELM327 Bluetooth client (see below)
    obd_pairing.h/.cpp       # Boot-time-only Bluetooth pairing/recovery screen (Device/Password/Connect); own temporary BluetoothSerial, never touches obd_client's
    obd_simulator.h/.cpp     # Scripted drive-cycle generator (OBD_SIMULATION_ENABLED builds only)
  logging/
    csv_logger.h/.cpp        # Session files, buffered writes, capacity pruning, log summary/delete
  storage/
    sd_manager.h/.cpp        # SD mount + touch-calibration persistence (pre-existing)
  system/
    connection_state.h       # ConnectionState enum (header only)
    status_led.h/.cpp        # Onboard RGB LED: shift-flash (priority) / MIL steady red / off
    dtc_decoder.h/.cpp       # Mode 03/07 DTC byte-pair decoding
    dtc_lookup.h/.cpp        # Binary-search DTC description lookup; queries dtc_table.h
    config_store.h/.cpp      # Config page settings (theme, gauge calibration, user vars, logging), persisted to /config.txt
```

Notable deviations from the original proposal, and why:

- **No `dashboard_renderer`/`boot_player` split yet.** Boot image drawing still lives in the pre-existing `display_manager.cpp`; the new gauge/page rendering went into `cluster_pages.cpp` + `gauge_widgets.cpp` instead of a single `dashboard_renderer`, since the UI cluster guide's six-page spec didn't map cleanly onto one renderer file.
- **`obd_client` runs on its own FreeRTOS task**, not a non-blocking state machine driven from `loop()`. `BluetoothSerial::connect()` and every ELM327 command round-trip are blocking calls with multi-second worst cases; rather than build a hand-rolled AT-command scheduler that still bottoms out on a blocking `connect()`, `ObdClient::begin()` starts a task pinned to `config::kObdTaskCore` (core 0, away from the Arduino loop task). The render loop reads a mutex-guarded `TelemetrySnapshot` copy every iteration and never touches Bluetooth directly, which satisfies "never block the display loop on Bluetooth I/O" more directly than a cooperative scheduler could on this library.
- **No `scheduler.h`.** `loop()` uses plain `millis()`-delta checks per subsystem (touch poll, UI refresh, CSV row/flush), matching the coding standards' preference for plain direct code over a scheduling abstraction at this project's size.
- **`config_store` and `dtc_decoder`** were not in the original proposal; they exist to back the config pages' persisted settings and the Diagnostics page's DTC list, respectively.
- **`obd_simulator`** is bench-only. Building the `cyd_4inch_sim` environment defines `OBD_SIMULATION_ENABLED`, which compiles `BluetoothSerial` and the whole AT/PID path out of `obd_client.*` and runs `ObdClient::simulationLoop()` on the same task instead. Because the swap happens behind `ObdClient`'s existing public interface, `main.cpp`, `cluster_pages`, `touch_handler`, and `csv_logger` are untouched and the mutex/task timing under test matches production.
- **`obd_pairing`** was not in the original proposal; it replaces what was originally a hard boot-time halt on a missing/invalid `/obd_config.txt` with a recovery UI. It deliberately does *not* live inside `ObdClient`/`obd_client.cpp` - it runs synchronously in `main.cpp`'s `setup()`, entirely before `ObdClient::begin()` is called, using its own `BluetoothSerial` instance. This keeps `ObdClient::taskLoop()`'s existing infinite-retry-with-backoff behavior completely untouched for the normal runtime case (a connection dropped while the dashboard is showing), and confines the new scan/pair/write-config logic to the one-time boot decision of *which* credentials to hand to `ObdClient::begin()` in the first place. Like `obd_simulator`, it compiles out entirely under `OBD_SIMULATION_ENABLED`.

## Simulated Telemetry Build

`env:cyd_4inch_sim` replaces the ELM327 link with a scripted drive cycle so the entire UI can be exercised with no adapter, no vehicle, and no Bluetooth pairing. It inherits every flag from `env:cyd_4inch` and adds only `-D OBD_SIMULATION_ENABLED=1`; dropping the Bluetooth stack also cuts roughly 770 KB of flash.

```powershell
pio run -e cyd_4inch_sim --target upload
```

Design rules for this mode:

- The swap is **compile-time and confined to `ObdClient`**. No consumer of telemetry may branch on `OBD_SIMULATION_ENABLED`; if a page or logger needs to know, the design is wrong.
- Simulated values are **deterministic** (fixed-seed LCG jitter, no hardware entropy) so a rendering regression reproduces identically between runs.
- The simulator runs on the same FreeRTOS task and core as the real client and publishes through the same mutex, so timing and concurrency behavior under test match production.
- `SD_LOGGING_ENABLED` stays **orthogonal**: simulated builds log to SD normally, which doubles as a dense exercise of `CsvLogger`.

The cycle is a fixed segment table in `obd_simulator.cpp` (idle, three gear pulls, cruise, highway, decel fuel cut, stop-and-go, idle) lasting about 95 seconds. RPM, speed, and throttle are interpolated from the table; every other PID is derived from those three so no two readings can contradict each other. Segments are chosen to reach specific UI states — the second pull crosses the shift light, the third reaches the redline arc, and the decel segment produces fuel-cut values on Page 3.

Tunables live in the `kSim*` block of `src/app_config.h`:

| Constant | Purpose |
| --- | --- |
| `kSimTimeScale` | Multiplies the cycle clock; override per build with `-D SIM_TIME_SCALE=2.0F` |
| `kSimConnectDelayMs` | Simulated handshake before the badge reaches `Live` |
| `kSimReconnectBlipEnabled` and `kSimBlip*Ms` | Periodic dropout that walks the badge through `Live` -> `Stale` -> `Reconnecting` -> `ObdConnecting` -> `Live` |
| `kSimWarningSweep*` | Periodically drives coolant and voltage past their warning thresholds |
| `kSimDtcAppearAfterMs` | Delay before the MIL and the fake DTC list latch |

The reconnect blip is scheduled from real `millis()`, never `kSimTimeScale`, so stale detection stays true to `kTelemetryStaleThresholdMs`. The cycle clock freezes for the duration of a blip so gauges resume mid-segment instead of jumping.

## Application Lifecycle

Implement the lifecycle as an explicit state machine rather than a sequence of delays:

```text
Boot -> SdInit -> DisplayReady -> BootAsset -> ObdConnecting -> Live
                                       |              |             |
                                       v              v             v
                                    Degraded <---- Reconnecting <- Stale
```

1. Initialize serial output at the monitor speed configured by PlatformIO (`115200`).
2. Mount the SD card and load persisted settings (`/config.txt`) - including the saved display orientation - before the display initializes below, so orientation is correct from the first frame instead of needing a second correction pass. A failed mount falls back to defaults; this early mount is independent of `SD_LOGGING_ENABLED` (see step 5).
3. Initialize the display in the persisted orientation, set landscape rotation, clear the screen, and draw a minimal status screen.
4. Load the OBD adapter identity from `/obd_config.txt` (mandatory; no fallback default). **As built**, a missing/invalid file, or stored credentials that fail to connect `kPreflightMaxAttempts` (5) times in a row, no longer halts boot - it falls into an on-device Bluetooth pairing/recovery screen (`obd_pairing::run()`, see `src/obd/obd_pairing.h`) that scans for nearby adapters, lets the user pick a device + a common PIN, and writes a fresh `/obd_config.txt` on a successful connect. This runs entirely before step 5 below, using its own temporary `BluetoothSerial` instance so `ObdClient`'s runtime connect/retry/backoff logic is never touched; it is boot-time only and is never re-entered once the dashboard is showing.
5. Start Bluetooth and attempt the configured ELM327 connection.
6. Play the selected boot asset, but wait for OBD connection or timeout (15s max) while showing live status updates at the bottom of the screen. **As built**, this step has been reordered: Bluetooth start (step 5) now happens *before* the boot-asset display, and the boot screen blocks on `ObdClient::getConnectionState() == Live` or timeout, redrawing the latest connection message (e.g. "Connecting by MAC...", "Bluetooth link established") via `DisplayManager::drawBootStatus()` — replacing the originally-specified flat finite duration with a connection-aware bounded wait.
7. Initialize touch calibration and start SD CSV logging (using the card already mounted in step 2) only when `SD_LOGGING_ENABLED` is defined. Unavailable logging must not affect dashboard functionality.
8. Poll supported PIDs on a schedule, publish complete telemetry snapshots, and enter `Live` only after valid engine data arrives.
9. On timeout or disconnect, retain the last value only as stale data, render the stale/disconnected state clearly, and retry with bounded backoff.

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

Store telemetry in native Imperial units at decode time (speed in MPH, temperature in °F, pressure in kPa), then apply display-unit conversions and CSV logging conversions independently at render/write time. This avoids re-decoding OBD responses and keeps decode logic simple. Do not assume every ECU supports every PID. Each field requires a validity flag and last-updated timestamp so an unsupported or timed-out PID cannot be displayed as a genuine zero.

**Display Unit System** (`UI` config page, `useMetricUnits` setting): Controls how speed, temperature, and pressure are rendered on all dashboard pages and config page fields. Metric mode displays km/h, °C, and kPa; Standard mode displays mph, °F, and psi. This setting also adjusts stepper increment sizes on the GAUGES and USER VARS pages so numeric entry feels natural in the chosen unit system.

**CSV Logging Unit System** (`LOGS` config page, `useMetricLogs` setting): Controls the schema and unit values written to SD card. Independent of the display setting — you can view the dashboard in mph while logging in km/h, or vice versa. Toggling this setting stages a pending change; **deletion happens when the change is saved** (SAVE TO SD button), and **only if the unit system actually changed since the last save**. Toggling back to the original value before saving leaves existing logs untouched. This ensures each session file uses one consistent unit system from header to last row.

A suitable snapshot shape is:

```cpp
struct TelemetryValue {
  float value;
  bool valid;
  uint32_t updatedAtMs;
};

struct TelemetrySnapshot {
  TelemetryValue rpm;
  TelemetryValue speedMph;          // Stored in MPH (converted from km/h at decode time)
  TelemetryValue coolantF;          // Stored in °F (converted from °C at decode time)
  TelemetryValue engineLoadPercent;
  TelemetryValue iatF;              // Stored in °F (converted from °C at decode time)
  TelemetryValue throttlePercent;
  bool connected;
  uint32_t capturedAtMs;
};
```

**Note**: The actual implementation (`src/obd/telemetry.h`) stores speed in MPH and temperature in °F natively; conversions to metric units (km/h, °C, kPa) happen in two places:
- **Display rendering** (`src/display/cluster_pages.cpp`): Uses `units::displaySpeed()`, `units::displayTemp()`, etc. to convert based on the UI config page's `useMetricUnits` setting.
- **CSV logging** (`src/logging/csv_logger.cpp`): Uses the same conversion helpers based on the LOGS page's `useMetricLogs` setting to write the correct unit schema and values.

This split design keeps OBD decode logic simple (no dual-path conversions) while allowing independent control of the display and logging unit systems.

Treat the snapshot as the renderer and logger boundary. The OBD client updates it only after parsing a response; the renderer must not perform Bluetooth I/O. Poll high-priority values such as RPM and speed more frequently than secondary PIDs, while avoiding adapter overload. Start with measured refresh behavior and tune using serial timing data rather than hard-coded optimistic intervals.

**As built**, `ObdClient` polls the full set of standard Mode 01 PIDs a 2006 Mustang GT (4.6L 3V) exposes over generic OBD-II — RPM and speed every cycle, plus one of the remaining 16 PIDs round-robined per cycle (monitor status/MIL+DTC count, engine load, coolant, STFT/LTFT bank 1, MAP, timing advance, IAT, MAF, throttle, O2 B1S1/B2S1, fuel rail pressure, fuel level, barometric pressure, and control module voltage — see `src/obd/obd_pids.cpp` for the exact PID bytes and decode formulas). Mode 03/07 (DTC read) and Mode 04 (clear codes) are issued on demand from the Diagnostics page, not on the polling cycle. Each connection-stage message (e.g. "Connecting by MAC...", "Bluetooth link established", "ELM327 initialized; polling PIDs") is now both appended to `/logs/connection.log` (`src/logging/connection_logger.h/.cpp`) and cached in `ObdClient::lastStatusMessage_` (mutex-guarded, mirroring `snapshot_`) via `ObdClient::getLastStatusMessage()`, which `main.cpp` polls during boot to drive `DisplayManager::drawBootStatus()`, so failed connections can be diagnosed without a serial monitor.

The adapter's identity is not configured on-device — there is no config page for it. Instead, `loadObdCredentials()` (`src/obd/obd_credentials.h/.cpp`) reads `mac`/`id`/`password` from `/obd_config.txt` on the SD card once at boot (see `docs/sd_card_templates/obd_config.example.txt` for the install-time template). **All three fields are mandatory — there is no fallback default.** `main.cpp` calls `loadObdCredentials()` immediately after display init (before the boot animation, touch calibration, or SD logging start) and, if it returns `false` (SD not mounted, file missing/unreadable, or any of `mac`/`id`/`password` blank or invalid), calls `DisplayManager::showFatalError()` to show a red "OBD CONFIG ERROR" screen and then halts in an infinite `delay()` loop — `loop()` is never entered, and `ObdClient::begin()` is never called. This mandatory check is skipped only for `OBD_SIMULATION_ENABLED` builds (`cyd_4inch_sim`), which never use these credentials. When credentials load successfully, `ObdClient::taskLoop()` connects by MAC address (`BluetoothSerial::connect(uint8_t[6])`) — `mac` is guaranteed present at that point, so the device-name (`id`) connect path only remains as defensive fallback code.

**Units feature** (added post-spec): Both the UI and CSV logging support independent metric/standard toggles. Display units are configured on the UI config page and affect how speed, temperature, and pressure are rendered on all dashboard pages and stepper increments on config pages. CSV logging units are configured independently on the LOGS config page; changing this setting deletes all existing log files to prevent unit-mixed rows. See `src/system/units.h` for the conversion helper library and the UI & SD Logging guides for detailed mode documentation.

**DTC descriptions feature** (added post-spec): The Diagnostics page displays human-readable descriptions alongside every DTC code by looking them up in a flash-resident, binary-searchable table generated from `src/data/Mustang_DTC.csv` (see `src/scripts/csv_to_dtc_table.py` for the generation pipeline — standard Python `csv` module, no extra dependencies). Each code row on the display shows `CODE  Description` in a smaller proportional font (`FreeSans9pt7b`). Descriptions longer than the visible line are automatically truncated with `...` and become tappable to open a detail overlay with the full description word-wrapped; tapping the overlay again closes it. Codes not found in the lookup table (manufacturer-specific or unmapped) display as a bare code and are not tappable. See the [UI Cluster Guide's Page 5 section](cyd-obd2-ui-cluster-guide.md#page-5--diagnostics-dtc-reader) for the complete on-screen behavior and truncation/overlay mechanics.

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

The current static boot image is stored as a losslessly-compressed indexed PNG PROGMEM byte array (`src/assets/boot_logo_png.h`, ~72KB vs. ~300KB for the raw RGB565 array it replaced) and decoded scanline-by-scanline at boot via PNGdec (`DisplayManager::drawBootImage()`), pushing each line to the display with `TFT_eSPI::pushImage()`. The `PNG` decoder object is heap-allocated only for the duration of that call and freed immediately after — its internal zlib window/pixel buffers (~39KB) are only needed once at boot, before the Bluetooth stack and SD/touch subsystems claim heap, and this board has no PSRAM to spare holding that buffer permanently. (`BOOT_RGB666_ASSETS_AVAILABLE` names the planned animated-frame format described below, not the current static asset's in-memory layout.)

**Generating Boot Assets** (as-built implementation): A two-script pipeline in `src/scripts/` handles converting a source image into the `boot_logo_png.h` header:

1. `optimize_png.py` — quantizes a 480×320 source PNG to ≤256 colors (indexed palette), reporting per-channel color-drift metrics to ensure the quantization is accurate. Requires Python 3 + Pillow; fails (non-zero exit) if drift exceeds acceptable thresholds, preventing a silently mis-tinted asset.
2. `png_to_header.py` — reads a quantized PNG, parses its IHDR for metadata, and emits a PROGMEM byte array (`kBootLogoPng`, `kBootLogoPngSize`) in the exact format consumed by `DisplayManager::drawBootImage()`.

For generated boot image/frame data:

- Store generated assets in a clearly named source or include directory and document the generator/source format.
- After (re)generating an asset, verify it against its source image — for example, compare average per-channel values between the source and the decoded array — rather than trusting the conversion tool. A conversion bug that truncates or mis-packs pixels (such as silently dropping the high byte of every RGB565 value) still compiles and renders; it just shows up as an incorrect tint, not a build or runtime failure.
- Call `setSwapBytes(true)` before `pushImage()` for boot/animation frames on this panel. The array holds a standard (non-pre-swapped) RGB565 constant per pixel, and `pushImage`'s internal `pushPixels` path only emits the byte order this panel expects when swap is enabled. `fillScreen`/`fillRect` do not need this — they swap bytes internally.
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

Place user-adjustable values in `app_config.h`, including units, gauge ranges, warning thresholds, display/log intervals, and boot mode defaults. The adapter's mac/id/password are **not** among these and have no default in `app_config.h`: they're mandatory install-time values read once at boot from `/obd_config.txt` on the SD card (`loadObdCredentials()`, `src/obd/obd_credentials.h/.cpp`). A missing, unreadable, or incomplete file is fatal — the device shows an on-screen error and halts before `loop()` ever runs (see the OBD-II Integration section above), rather than falling back to any built-in default. Centralize on-screen display text (labels, units, button/status strings, page titles) in `src/labels.h` (`namespace labels`) to keep rendered content audit-able and localization-ready. Do not hardcode personal adapter addresses, PINs, Wi-Fi credentials, or tokens in tracked source. Provide an ignored local configuration header or documented build flags for sensitive machine-specific settings.

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
- Build `cyd_4inch_sim` and confirm it still links without `BluetoothSerial`; its flash usage should be roughly 770 KB lower than `cyd_4inch`. An unexplained size change in either environment usually means `platformio.ini` flags moved or went missing.
- Use **Upload and Monitor** for `cyd_4inch`, verify the ESP32 upload completes, and confirm runtime logs are readable at `115200` baud.
- Flash the board and verify the display reports 480 x 320 after selecting landscape rotation.
- Boot with valid and missing boot assets; both paths must reach the connection screen.
- Connect to a known ELM327 adapter and confirm RPM, speed, and at least one secondary PID update.
- Turn off the adapter or leave vehicle range; verify that the UI becomes stale/disconnected without freezing or reporting false zero values.
- Test an ECU/PID failure and verify only the affected field is invalid.
- Insert an SD card **with a valid `/obd_config.txt`**, confirm a new CSV has one header and parseable rows, then inspect it on a host machine. (Note: unlike CSV logging itself, `/obd_config.txt` is normally required on `cyd_4inch` to skip straight to the dashboard - see the pairing-screen checks below for what happens when it's missing.)
- Observe display rendering and Bluetooth recovery for an extended bench session; confirm no uncontrolled memory growth, watchdog resets, or UI stalls.
- With no SD card, or no `/obd_config.txt` on it, confirm `cyd_4inch` shows the Bluetooth pairing screen (DEVICE / PASSWORD / CONNECT buttons) instead of halting, and that it finds the bench ELM327 adapter during its scan (filtered list if the adapter's name matches a known pattern, full list otherwise). Confirm `cyd_4inch_sim` boots normally in the same scenario, since it skips this check entirely.
- On the pairing screen, tap DEVICE and PASSWORD to cycle through the discovered devices and the four common PINs, then tap CONNECT with the correct combination; confirm `/obd_config.txt` is written with the expected `mac=`/`id=`/`password=` values and the dashboard proceeds to boot normally (`obdClient.begin()` picks up the just-verified credentials).
- Create `/obd_config.txt` missing one of `mac=`/`id=`/`password=` (or with a malformed `mac=`) and confirm the same pairing-screen fallback (not a halt).
- Put a **valid-format but wrong** `mac=`/`password=` in `/obd_config.txt` (a real field format, but not the adapter actually in range) and confirm the serial/connection log shows 5 failed preflight connect attempts before the pairing screen appears.
- On the pairing screen, deliberately select the wrong PASSWORD against a real adapter and tap CONNECT 5 times; confirm the screen resets its selection and re-scans automatically rather than getting stuck, and that this is logged to `/logs/connection.log`.
- Create a fully valid `/obd_config.txt` (`mac=`/`id=`/`password=` all set, matching a reachable adapter) and confirm the serial log shows a MAC-address connect attempt and the status badge reaches LIVE, with no pairing screen shown and no more than one preflight connect attempt.
- On the Config: UI page, toggle Flip Screen and tap Save; confirm the device restarts, re-runs touch calibration automatically, and boots with the display and boot logo rotated 180° with taps landing correctly afterward. Toggle back and confirm it returns to normal, and that the setting survives a full power cycle either way.
- On the Config: UI page, tap the Touch Calibration button twice (arm, then confirm) and verify the device restarts and re-runs the 4-corner calibration routine immediately, without needing a Save tap; confirm touch accuracy afterward.

## Safety and Scope Notes

This is an informational dashboard, not a vehicle control system. It must not transmit commands that modify vehicle behavior, distract from driving, or encourage interaction while moving. Keep vehicle-specific assumptions configurable and test only in a safe, stationary environment before road use.

## References

- [CYD OBD-II UI Cluster Guide](cyd-obd2-ui-cluster-guide.md)
- [CYD OBD-II SD Card Telemetry Logging Guide](cyd-obd2-sd-logging-guide.md)
- [OBD adapter credentials file template](sd_card_templates/obd_config.example.txt)
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