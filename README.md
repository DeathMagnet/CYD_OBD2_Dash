# CYD OBD-II Dashboard

A feature-rich, real-time automotive dashboard for the **Hosyond 4.0-inch ESP32-32E CYD** with a 320×480 ST7796S TFT, used in **480×320 landscape** orientation. Displays live OBD-II data with configurable themes, SD card CSV logging, DTC read/clear, and a fully customizable on-device configuration system—purpose-built for Ford Mustang enthusiasts.

## Hardware Requirements & Wiring

### Required Components

| Component | Description |
|---|---|
| Hosyond 4.0″ ESP32-32E CYD | ESP32-D0WD-V3 + 4″ 320×480 ST7796S TFT, used at 480×320 landscape, with resistive touch |
| ELM327 Bluetooth | OBD-II adapter, v1.5 or later (avoid cheap clones) |
| MicroSD card | FAT32 formatted, ≤32 GB recommended |
| 12V → 5V USB adapter | Powers the CYD from the vehicle's OBD port or 12V rail |

### CYD Pin Reference

The Hosyond 4.0″ ESP32-32E CYD uses the following display and SD-card pins. The display uses the ST7796S controller over SPI and the dashboard uses it in landscape orientation.

| Function | GPIO |
|---|---|
| TFT MOSI | GPIO 13 |
| TFT MISO | GPIO 12 |
| TFT SCLK | GPIO 14 |
| TFT CS | GPIO 15 |
| TFT DC | GPIO 2 |
| TFT BL (backlight) | GPIO 27 |
| Touch CS | GPIO 33 |
| LED Red | GPIO 22 |
| LED Green | GPIO 16 |
| LED Blue | GPIO 17 |
| SD CS | GPIO 5 |
| SD MOSI | GPIO 23 |
| SD MISO | GPIO 19 |
| SD CLK | GPIO 18 |

> **Note on SPI buses:** The SD card uses its own SPI bus (VSPI, via `SdManager`), while the TFT and resistive touch controller share a separate SPI bus (HSPI, enabled by the `USE_HSPI_PORT` build flag in `platformio.ini`). Both buses can then operate simultaneously without conflict. Without `USE_HSPI_PORT`, `TFT_eSPI` defaults to the same VSPI peripheral the SD card uses; GPIO output signals (MOSI/SCLK) fan out to both sets of pins so drawing still appears to work, but the shared peripheral's MISO **input** can only listen to one bus's pins at a time — whichever `begin()` call runs last wins. Since `SdManager::begin()` runs after the display/touch init, it silently steals the touch controller's read line, and touch input stops responding even though the screen keeps rendering normally.

> **Note on the RGB LED:** The onboard LED is active-low (GPIO state LOW = LED on, HIGH = LED off).

### Wiring Overview Diagram

```
Vehicle OBD-II Port
        │
   ELM327 Adapter  ──── Bluetooth ────►  ESP32 (CYD)
                                              │
                                         ST7796S TFT  ──► 480×320 landscape display
                                              │
                                         SD Card Slot ──► CSV logging
                                              │
                                      12V→5V USB adapter (from OBD port pin 16)
```

## OBD-II Adapter Setup

The ELM327 adapter's Bluetooth identity is **not** configured on-device — there's no config page for it. Instead, it's required to be present on the SD card and is read once at boot from a plain-text file. **All three fields are mandatory** — there is no fallback default for any of them:

1. Copy [docs/sd_card_templates/obd_config.example.txt](docs/sd_card_templates/obd_config.example.txt) to the **root of the SD card** and rename it to `obd_config.txt` (the path must be exactly `/obd_config.txt`).
2. Edit it with your adapter's details — `mac`, `id`, and `password` must all be set:
   ```
   mac=AA:BB:CC:DD:EE:FF
   id=OBDII
   password=1234
   ```
   - `mac` (**required**): your adapter's Bluetooth MAC address (colon-, dash-, or un-separated hex all work). The dashboard always connects by address, never by scanning for a device name, since it's more reliable.
   - `id` (**required**): the Bluetooth SPP device name, used for identification/logging (common values: `OBDII`, `OBDLink`, `Vgate`, `VEEPEAK`, `OBD2`).
   - `password` (**required**): the legacy numeric Bluetooth pairing PIN (common values: `1234`, `0000`).
3. Re-insert the card and power on. **If the file is missing, unreadable, or any of the three fields is blank/invalid, the device shows a red on-screen error ("OBD CONFIG ERROR") and halts boot** — it never reaches the dashboard, touch calibration, or SD logging. Fix the file and reboot.

To change credentials later, re-edit `/obd_config.txt` on the card (on a computer, or in-place) and reboot — there's no live reconnect from the UI.

**Exception**: the `cyd_4inch_sim` bench/demo build (see [Build Environments](#build-environments)) skips this requirement entirely, since it never connects to a real adapter.

### SD Card Contents

The dashboard reads/writes these files at the SD card root:

| File | Purpose |
| --- | --- |
| `/obd_config.txt` | OBD-II adapter identity (`mac`/`id`/`password`), read once at boot. See above. |
| `/config.txt` | Dashboard settings (theme, gauge ranges, units, etc.), written by the Config pages' Save button. |
| `/touch_cal.dat` | Touch calibration data, written by the calibration routine. |
| `/obd_log_*.csv` | CSV telemetry logs (one file per session), written when `SD_LOGGING_ENABLED` is set. |

## Build Environments

| Environment | Purpose |
| --- | --- |
| `cyd_4inch` | Default. Live telemetry over Bluetooth from an ELM327 adapter. |
| `cyd_4inch_sim` | Bench/demo. Replaces the adapter with a scripted drive cycle, so the whole UI can be exercised with no adapter or vehicle connected. |

```powershell
pio run -e cyd_4inch_sim --target upload
```

The simulated ~95-second cycle runs idle → three gear pulls (crossing the shift light and redline) → cruise → decel fuel cut → stop-and-go → idle, and periodically drops the link so the badge walks LIVE → STALE → RECONNECTING → CONNECTING → LIVE. Page 5 (Diagnostics) serves a fixed set of fake DTCs that "Clear Codes" clears. Tunables live in the `kSim*` block of [src/app_config.h](src/app_config.h); add `-D SIM_TIME_SCALE=2.0F` to the environment's `build_flags` to sweep the cycle at double speed. SD logging is independent of simulation mode and stays enabled.

### Serial Debugging

To open a serial monitor and watch live debug output, use PlatformIO's device monitor. The baud rate is configured once in the shared `[env]` block of `platformio.ini` at 115200:

```powershell
pio device monitor
pio device monitor -e cyd_4inch_sim
```

To build, upload, and immediately open the monitor in one step:

```powershell
pio run -e cyd_4inch_sim -t upload -t monitor
```

### Generating Assets

The boot splash image is stored as a compressed indexed PNG in `src/assets/boot_logo_png.h` and decoded at runtime via the `PNGdec` library. To replace or regenerate the boot splash, follow this two-step pipeline:

**Step 1: Quantize the source PNG**

Your source image must already be exactly **480×320 pixels**. Quantize it to an indexed (≤256-color) PNG:

```powershell
python src/scripts/optimize_png.py <source.png> -o <quantized.png> --width 480 --height 320 --max-colors 256
```

**Requirements:**
- Requires Python 3 + Pillow: `pip install pillow`
- If the quantized output drifts from the source by more than `--max-avg-delta` (default 4.0 per channel) or `--max-channel-delta` (default 24 per channel), the script fails (non-zero exit) and prints a color-delta report so you can see where the drift happened. Adjust the source colors or raise these thresholds as needed.

**Step 2: Convert to a PROGMEM C header**

```powershell
python src/scripts/png_to_header.py <quantized.png> -o src/assets/boot_logo_png.h --name boot_logo
```

This generates the exact PROGMEM byte array (`kBootLogoPng`, `kBootLogoPngSize`) consumed by `DisplayManager::drawBootImage()`. The header comment includes the original PNG's dimensions and color type, and notes the space savings vs. raw RGB565.

**Step 3: Rebuild and upload**

No build flags need to change. `DisplayManager::drawBootImage()` (`src/display/display_manager.cpp:39-60`) includes `src/assets/boot_logo_png.h` directly and decodes it via PNGdec at boot time.

## UI Overview

### Navigation

The header bar at the top of every page (40px tall) contains these elements, left to right:

- **Previous-page arrow** (`<`): Tap to step backward within the current group (dashboard or config), wrapping around to the last page in the group.
- **Page title**: Centered, short name only (e.g., "PRIMARY CLUSTER", "UI", "GAUGES") — no page numbers are shown on-device.
- **OBD badge**: A rounded pill showing the current Bluetooth connection state:
  - `LIVE` (green) — connected and receiving data
  - `CONNECTING` / `RECONNECTING` / `DISPLAY READY` (blue-ish) — in-progress handshake
  - `BOOT` / `SD INIT` / `STALE` / `NO OBD` (red/crimson) — not connected or timed out
- **Check Engine warning** (`!!!`): Bold red text, displayed only when the MIL (Malfunction Indicator Lamp) is on. Tap while active to jump straight to the Diagnostics page. Inert (invisible) when MIL is off.
- **Configuration mode toggle**: Cog icon (⚙️) when on a dashboard page — tap to jump to the Config group, remembering which config page you last visited. Steering-wheel icon when on a config page — tap to jump back to your last dashboard page.
- **Next-page arrow** (`>`): Tap to step forward within the current group, wrapping to the first page.

### Dashboard Group (Pages 1–5)

Navigate with the prev/next arrows in the header; cycles within the dashboard group only.

#### Page 1 — Primary Cluster

The main at-a-glance engine gauges:

- **RPM gauge** (large arc, center): Displays engine speed (0 to "MAX RPM" setting). Arc color zones:
  - **Green/primary**: 0 to "SHIFT LIGHT RPM" threshold
  - **Orange/caution**: "SHIFT LIGHT RPM" to "REDLINE RPM" threshold
  - **Red/danger**: "REDLINE RPM" to "MAX RPM"
  - When RPM reaches "SHIFT LIGHT RPM", the text digits flash red at a 200 ms cadence (same as the onboard RGB LED shift-point flash).
- **Speed gauge** (bottom): 0 to "MAX SPEED" MPH (or km/h in metric mode).
- **Coolant temperature** (top-right corner): Raw temperature reading. Turns the warning color (red/crimson) when above the "COOLANT TEMP WARN" threshold.
- **IAT** (top center): Intake air temperature.
- **Throttle** (bottom-right): Horizontal bar, 0–100%.

#### Page 2 — Engine Load & Airflow

Air-intake and fuel-trim diagnostics:

- **Engine Load %** (arc): 0–100% load.
- **MAF (Mass Air Flow)** (g/s, with peak-hold): Grams per second; the displayed value holds at the maximum reading seen since the page was entered, then decays back to live after 5 seconds of inactivity.
- **Timing Advance** (degrees): Raw spark timing, in degrees before top dead center.
- **STFT Bank 1** (bar, 0–100%): Short-term fuel trim. The bar is rescaled such that ±"FUEL TRIM RANGE" maps to 0–100% fill:
  ```
  bar fill = (stftPct + fuelTrimRangePct) * (50.0 / fuelTrimRangePct)
  ```
  For example, with a 25% fuel-trim range, ±25% trim maps to the full bar, and 0% trim centers the bar at 50%.
- **LTFT Bank 1** (bar, 0–100%): Long-term fuel trim, same scaling as STFT.

#### Page 3 — Car-Specific Sensors

Engine and fuel-system parameters:

- **Battery Voltage** (volts): Raw supply voltage. Turns the warning color when below the "LOW VOLTAGE WARNING" threshold.
- **Fuel Rail Pressure** (psi or kPa): Raw MAP sensor reading fed to the fuel injectors.
- **Boost / Vacuum gauge** (single dynamic gauge):
  - Baseline: The "BOOST BARO BASELINE" (in psi) is converted to kPa. If MAP > baseline, the gauge shows **BOOST** in psi (or kPa metric), scaled to the "BOOST GAUGE MAX" setting. If MAP < baseline, it shows **VACUUM** in inHg (or kPa), scaled to "VACUUM GAUGE MAX".
  - Formula:
    ```
    baroKpa = kpaFromPsi(boroBaselinePsi)
    if (mapKpa > baroKpa)
        boost = mapKpa - baroKpa  [psi or kPa, 0 to BOOST_GAUGE_MAX]
    else
        vacuum = baroKpa - mapKpa  [inHg or kPa, 0 to VACUUM_GAUGE_MAX]
    ```
- **O2 B1S1 / O2 B2S1** (volts): Oxygen sensor readings from both banks.

#### Page 4 — Performance & Telemetry

Calculated performance estimates and a rolling airflow history:

- **EST. Horsepower** (HP):
  ```
  hp = mafGps * hpEstimationFactor
  ```
  Where `mafGps` is the current mass-air-flow reading and `hpEstimationFactor` is a user-tunable multiplier (default 0.80). This is a rough estimate based on MAF alone and does not account for fuel pressure, timing, or load.

- **EST. Torque** (ft-lb):
  ```
  torque = (rpm >= 500) ? hp * 5252.0 / rpm : 0
  ```
  Torque is blanked (shown as 0) below 500 RPM to avoid division artifacts. The 5252 constant is the standard conversion between HP, torque, and RPM.

- **0–60 Timer** (seconds, tap-to-reset):
  - Auto-starts when speed first rises above 0 MPH.
  - Auto-stops when speed reaches the "0-60 TARGET SPEED" threshold (default 60 MPH).
  - Tap the displayed time to manually reset the timer.

- **Intake Airflow (60-second rolling graph)** (MAF history):
  - Plots one MAF reading per second for the last 60 seconds (newest on the right).
  - Vertical axis auto-scales to the maximum value in the current window.
  - Useful for spotting MAF sensor lag or transient spikes during acceleration.

#### Page 5 — Diagnostics

Read, interpret, and clear OBD-II Diagnostic Trouble Codes (DTCs):

- **MIL status line**: Shows either `MIL: ACTIVE (ON)` or `MIL: INACTIVE (OFF)`.
- **Code list**: Automatically reads both stored codes (OBD Mode 03) and pending codes (OBD Mode 07) when the page is opened. Codes are decoded and displayed as P-codes, C-codes, B-codes, or U-codes (e.g., `P0101` for an MAF sensor range error). No description text is provided — look up the code online for details.
- **REFRESH CODES**: Re-read the code list from the adapter immediately.
- **CLEAR CODES**: Two-tap confirm pattern. First tap arms the button (it highlights in the warning color and changes its label to "TAP TO CONFIRM"); tapping again within 5 seconds sends OBD Mode 04 (clear DTCs) to the adapter. The confirmation times out after 5 seconds if not confirmed.

### Config Group (UI, GAUGES, USER VARS, LOGS)

Navigate with the prev/next arrows; cycles within the config group only. All settings persist to `/config.txt` on the SD card and are applied immediately on save (no reboot required unless otherwise noted).

#### UI Page

- **Active Theme** (tap-to-cycle): Cycles through `Modern Flat` → `Neon` → `S197` → (wraps). Determines the color palette for gauges, fonts, and the MIL/shift-light warning colors. The entire visible page recolors immediately.
- **Units** (tap-to-toggle): `STANDARD (MPH/°F/PSI)` ↔ `METRIC (KM/H/°C/KPA)`. Affects all displayed temperatures, pressures, and distances. The displayed gauge values and axis labels update immediately, but the internal configuration never changes — only the display multipliers swap.
- **Gauge Ticks** (tap-to-cycle): `TICS OFF` → `INSIDE TICS ONLY` → `OUTSIDE TICS ONLY` → `INSIDE AND OUTSIDE TICS` → (wraps).
  - **Constraint**: If the active theme is "S197", only the first two options are available (`OFF` ↔ `INSIDE ONLY`), because the S197 theme's bezel art lacks space for outer tick marks. Attempting to cycle past `INSIDE ONLY` wraps back to `OFF`.
- **Touch Calibration** (tap-to-confirm): Immediately re-runs the 4-corner touch calibration routine — unlike the other UI settings, this isn't staged; it acts as soon as you confirm, independent of the Save button. First tap arms the button ("TAP TO CONFIRM", same pattern as Delete All Logs); tapping again within 5 seconds deletes the saved calibration and restarts the device, which then walks you through the calibration screen on boot.
- **Flip Screen** (tap-to-toggle): `NORMAL` ↔ `FLIPPED 180`. Rotates the display 180° for boards mounted upside-down behind the gauge cluster.
  - **⚠️ Warning**: Changing this setting **restarts the device** when you save, and re-runs touch calibration in the new orientation (same as the first-boot calibration routine). Make sure you're ready to re-tap the 4 calibration corners after saving.

#### GAUGES Page

Six rows, each a `-` / `+` stepper (no free-text entry). All values are continuously validated and clamped as you adjust them.

| Setting | Default | Min | Max | Step | Notes |
|---|---|---|---|---|---|
| **Shift Light RPM** | 5800 | 3000 | 6800 (clamped to ≤ current Redline RPM) | 100 | Threshold at which the LED flashes and Page 1's RPM digits begin flashing red. |
| **Redline RPM** | 6200 | 5000 (clamped to ≥ current Shift Light RPM) | 7000 (clamped to ≤ current Max RPM) | 100 | Upper caution/danger zone boundary on Page 1's RPM gauge. Tick marks stop lighting past this threshold. |
| **Max RPM** | 7000 | 7000 (clamped to ≥ current Redline RPM) | 9000 | 100 | Full-scale value of Page 1's RPM arc gauge. |
| **Max Speed** | 200 | 120 | 260 | 10 | Full-scale value of Page 1's speed gauge (in MPH; metric displays equivalent km/h). |
| **Vacuum Gauge Max** | 30 | 15 | 30 | 1 | Full-scale value of the Page 3 vacuum bar when vacuum is present (in inHg, or kPa in metric). |
| **Boost Gauge Max** | 25 | 10 | 40 | 1 | Full-scale value of the Page 3 boost bar when boost is present (in PSI, or kPa in metric). |

**Important constraint**: The three RPM thresholds (Shift Light, Redline, Max RPM) are kept in the live invariant `Shift Light ≤ Redline ≤ Max RPM`. Each stepper clamps its own value against its neighbor(s) as you adjust; if you load settings from `/config.txt` that are out of order (e.g., a hand-edited file), the top-down ordering is re-enforced right after load: Redline is clamped ≤ Max RPM, then Shift Light is clamped ≤ Redline.

#### USER VARS Page

Six rows, each a `-` / `+` stepper. These are the tuning variables that feed into the calculated values displayed on the dashboard. All are continuously clamped to their min/max.

| Setting | Default | Min | Max | Step | Usage |
|---|---|---|---|---|---|
| **Boost Baro Baseline** | 14.7 psi | 12.0 | 15.5 | 0.1 | Page 3's boost/vacuum calculation splits on this pressure: if MAP > baseline, display BOOST; else display VACUUM. The baseline is also subtracted from MAP to compute the displayed boost/vacuum magnitude. Typical value is local atmospheric pressure (14.7 psi = 1 atm at sea level). |
| **Coolant Temp Warn** | 220 °F | 180 | 250 | 5 °F (metric step is a temperature delta, 2.78 °C, not the absolute-value formula) | Page 2: coolant temperature reading turns the warning color when above this threshold. |
| **Low Voltage Warning** | 11.5 V | 9.0 | 13.0 | 0.1 | Page 3: battery voltage reading turns the warning color when below this threshold. (Unit-agnostic; no metric variant.) |
| **0-60 Target Speed** | 60 | 40 | 100 | 5 | Page 4: the 0–60 timer auto-stops when speed reaches this threshold. Useful if your car's speedo reads high/low or you want to test 0–100 instead. |
| **HP Estimate Factor** | 0.80 | 0.5 | 1.2 | 0.05 | Page 4: `hp = mafGps * hpEstimationFactor`. Adjust this multiplier to match a dyno number or known peak HP. Unitless; typically 0.75–1.0 depending on engine. |
| **Fuel Trim Range** | 25 | 10 | 50 | 5 | Page 2: the ±bound of the STFT/LTFT bar-gauge display range. A 25% range means the bar spans ±25% trim, so 0% trim centers the bar at 50% and ±25% trim pegs it at the ends. (Percentage; no metric variant.) |

#### LOGS Page

Configuration for SD-card CSV logging and log management.

- **Log Interval** (tap-to-cycle): `{50, 100, 250, 500, 1000}` ms, default 100 ms. This is a closed set (not a free numeric range). Cycles through the available options.
  - Smaller intervals = more granular data but larger files and more SD I/O.
  - Larger intervals = smaller files but coarser time resolution.

- **Log Units** (tap-to-toggle): `STANDARD` ↔ `METRIC`.
  - **⚠️ Warning**: Changing this setting **deletes all existing log files** when you save. This is by design, since files logged in one unit system mixed with files in another are hard to parse. Make sure you back up or export any logs before changing units.

- **Live log summary**: Shows the current number of log files and their total size in MB (e.g., `"LOGS, 42  MB, 1.2"`). Rescanned every 2 seconds, or immediately after a delete or a mode change that wipes logs.

- **Delete All Logs**: Two-tap confirm button (same pattern as "Clear Codes" on the Diagnostics page). First tap highlights the button in the warning color and changes the label to "TAP TO CONFIRM"; tap again within 5 seconds to actually delete all CSV log files. Confirmation times out after 5 seconds.

#### Shared Save Button

Appears at the bottom of every config page:

- **Color/state**:
  - **Default color**, label "SAVE TO SD": All current settings match what's saved to `/config.txt`. Nothing to save.
  - **Green**, label "SAVE TO SD": One or more settings differ from `/config.txt`. Tap to commit them.
  - **Default color**, label "SAVED!": Just after you tap Save, the page shows this feedback for 1.5 seconds, then reverts to the clean state.

- **Behavior**: Tapping Save while any setting is dirty (different from `/config.txt`) writes all 17 keys to `/config.txt` in `key=value` format, line by line. If any setting is out of range or invalid (e.g., from a hand-edited config file), it is silently clamped to the valid range before saving—an out-of-range value can never persist.

- **For Flip Screen**: Saving while this setting is dirty deletes the saved touch calibration and restarts the device so the new orientation and a fresh touch calibration both take effect together.

- **Touch Calibration is the exception to "applied on save"**: it's not a persisted setting and doesn't wait for SAVE TO SD — confirming the tap-to-confirm button deletes the saved calibration and restarts the device immediately.

## Status LED Behavior

The onboard RGB status LED provides real-time visual feedback for shift point and check-engine conditions:

- **Shift-point flash** (priority): When RPM reaches or exceeds the **Shift Light RPM** threshold (configured on the Config: GAUGES page, default 5800), the LED flashes the active theme's warning color at a **200 ms cadence** — the same flash rate as the Page-1 RPM digit shift indicator. This flash takes priority and remains visible even if the Check Engine light is also on, so you never miss the shift point cue.

- **Check Engine steady** (no shift): If the Check Engine (MIL) light is on but RPM is below the Shift Light threshold, the LED glows a steady **red**.

- **Off**: Otherwise, the LED is off.

**Theme-specific shift-flash colors and LED quantization:**
- **Modern Flat theme**: Shift flash is pure red (`0xD800`); MIL red is also pure red — the LED renders both as red, distinguishable only by the flash cadence. Prioritizes the flash's clear on/off visibility.
- **Neon theme**: Shift flash is hot orange (`0xFDA0`), which the LED quantizes to red + green (amber-ish); MIL red is pure red. Provides visual separation between shift and MIL even though the shift flash takes priority.
- **S197 theme**: Shift flash is amber-orange (`0xF8C0`), which quantizes down to pure red on the LED's 3 fixed on/off channels (only the red channel exceeds the half-scale threshold). Like Modern Flat, both shift and MIL render as red; the flash cadence tells them apart.

All LED updates respect the active-low logic: `LOW` = channel on, `HIGH` = channel off.
