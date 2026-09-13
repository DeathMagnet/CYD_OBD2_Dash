A feature-rich, real-time automotive dashboard for the **Hosyond 4.0-inch ESP32-32E
CYD** with a 320×480 ST7796S TFT, used in **480×320 landscape** orientation. It is
purpose-built for Ford Mustang enthusiasts and displays live OBD-II data with a
Mustang-themed UI, configurable boot splash image, SD card logging, and a
fully configurable theme system.

## Hardware Requirements & Wiring

### Required Components

| Component | Description |
|---|---|
| Hosyond 4.0″ ESP32-32E CYD | ESP32-D0WD-V3 + 4″ 320×480 ST7796S TFT, used at 480×320 landscape, with resistive touch |
| ELM327 Bluetooth | OBD-II adapter, v1.5 or later (avoid cheap clones) |
| MicroSD card | FAT32 formatted, ≤32 GB recommended |
| 12V → 5V USB adapter | Powers the CYD from the vehicle's OBD port or 12V rail |

### CYD Pin Reference

The Hosyond 4.0″ ESP32-32E CYD uses the following display and SD-card pins. The display
uses the ST7796S controller over SPI and the dashboard uses it in landscape orientation.

| Function | GPIO |
|---|---|
| TFT MOSI | GPIO 13 |
| TFT MISO | GPIO 12 |
| TFT SCLK | GPIO 14 |
| TFT CS | GPIO 15 |
| TFT DC | GPIO 2 |
| TFT BL (backlight) | GPIO 27 |
| Touch CS | GPIO 33 |
| SD CS | GPIO 5 |
| SD MOSI | GPIO 23 |
| SD MISO | GPIO 19 |
| SD CLK | GPIO 18 |

> **Note:** The SD card uses its own SPI bus (VSPI, via `SdManager`), while the TFT and
> resistive touch controller share a separate SPI bus (HSPI, enabled by the
> `USE_HSPI_PORT` build flag in `platformio.ini`). Both buses can then operate
> simultaneously without conflict. Without `USE_HSPI_PORT`, `TFT_eSPI` defaults to the
> same VSPI peripheral the SD card uses; GPIO output signals (MOSI/SCLK) fan out to
> both sets of pins so drawing still appears to work, but the shared peripheral's MISO
> **input** can only listen to one bus's pins at a time — whichever `begin()` call runs
> last wins. Since `SdManager::begin()` runs after the display/touch init, it silently
> steals the touch controller's read line, and touch input stops responding even though
> the screen keeps rendering normally.

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

## Build Environments

| Environment | Purpose |
| --- | --- |
| `cyd_4inch` | Default. Live telemetry over Bluetooth from an ELM327 adapter. |
| `cyd_4inch_sim` | Bench/demo. Replaces the adapter with a scripted drive cycle, so the whole UI can be exercised with no adapter or vehicle connected. |

```powershell
pio run -e cyd_4inch_sim --target upload
```

The simulated ~95-second cycle runs idle → three gear pulls (crossing the shift
light and redline) → cruise → decel fuel cut → stop-and-go → idle, and periodically
drops the link so the badge walks LIVE → STALE → RECONNECTING → CONNECTING → LIVE.
Page 6 serves a fixed set of fake DTCs that "Clear Codes" clears. Tunables live in
the `kSim*` block of [src/app_config.h](src/app_config.h); add
`-D SIM_TIME_SCALE=2.0F` to the environment's `build_flags` to sweep the cycle at
double speed. SD logging is independent of simulation mode and stays enabled.

## Dashboard Pages

### Dashboard Group (Pages 1–5)
Navigate with prev/next arrows in the header; cycles within this group only.

- **Page 1 — Primary Cluster**: Large RPM gauge with real-time speed, coolant, IAT, and throttle readouts.
- **Page 2 — Engine Load**: Engine load percentage, MAF sensor graph, spark timing, fuel trim diagnostics.
- **Page 3 — Car-Specific**: Battery voltage, fuel rail pressure, vacuum/boost gauge, O2 sensor voltages.
- **Page 4 — Performance**: Calculated horsepower and torque, 0–60 timer, intake airflow rolling graph.
- **Page 5 — Diagnostics**: Read, decode, and clear OBD-II Diagnostic Trouble Codes (DTCs). Tapping the Check Engine Light (MIL) on any page jumps here.

### Config Group (Pages UI, GAUGES, USER VARS, LOGS, OBD ADAPTER)
Navigate with prev/next arrows in the header; cycles within this group only. All settings persist to `/config.txt` on the SD card.

- **UI**: Display-only page showing the active theme (placeholder for future theme switching).
- **GAUGES**: Configure Shift Light RPM and Redline RPM thresholds that control the warning arcs on Page 1.
- **USER VARS**: User-adjustable variables like Boost Baro Baseline (atmospheric pressure baseline for vacuum/boost calculation).
- **LOGS**: Configure SD card log write interval, view live log summary (file count and size), and delete all logs.
- **OBD ADAPTER**: Pick the ELM327 Bluetooth adapter name and pairing PIN from preset lists (OBDII/OBDLink/Vgate/VEEPEAK/OBD2 and 1234/0000/1111/6789); saving reconnects immediately using the new values, no reboot required.

A **shared Save button** appears at the bottom of every config page. It is **green** when any value differs from what's saved to SD, and **default color** when all settings match. Tap it to persist all changes to `/config.txt` and see "SAVED!" feedback.

### Mode Toggle
The header includes a mode toggle button: a **cog icon** (⚙️) when viewing dashboard pages (tap to switch to config), and a **steering wheel icon** (🧭) when viewing config pages (tap to switch to dashboard). The button remembers which page you were on in each group, so you don't lose your place when switching back and forth.
