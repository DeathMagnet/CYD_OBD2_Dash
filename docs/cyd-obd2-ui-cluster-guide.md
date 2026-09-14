# CYD OBD-II Digital Cluster & UI Implementation Guide

## Purpose

This document provides complete, technical design and implementation instructions for the touch-driven, multi-page digital cluster UI on the Hosyond 4.0-inch ST7796S display (480x320 landscape). It covers screen layout, gauge animation mechanics, rendering pipeline, theme engine, touch interaction, transition physics, and page specifications.

---

## ✅ Implementation Status

The dashboard displays all six dashboard pages (1–4 and 6) plus five dedicated config pages (UI, GAUGES, USER VARS, LOGS, OBD ADAPTER), with these deliberate deviations from the design above:

- **All on-screen text is centralized.** Every label, unit, button text, page title, and status string shown in this spec is sourced from `src/labels.h` (`namespace labels`) rather than hardcoded in drawing code. The source of truth for display text is the labels constants in that header file, not the literal strings you see documented below.
- **All three themes are implemented.** `getTheme()` (`src/display/theme.cpp`) returns the Mustang S197, Torque Neon, or Modern Flat palette for the corresponding `ThemeId`. The UI config page's "Active Theme" row has a tap-to-cycle button (mirroring the Units row) that rotates through all three themes (Modern Flat -> Torque Neon -> Mustang S197 -> Modern Flat), persists the choice, and immediately recolors the visible page.
- **Page organization: Dashboard group vs. Config group.** Dashboard pages (1–4, 6) form one group; config pages (UI, GAUGES, USER VARS, LOGS, OBD ADAPTER) form another. The header includes a mode toggle button: a steering wheel icon when viewing dashboard pages (tap to switch to the last-visited config page), and a cog icon when viewing config pages (tap to switch to the last-visited dashboard page). Prev/next navigation arrows cycle only within the active group, never across both groups.
- **Direct-to-TFT rendering, not sprites.** This board's ESP32-32E has no PSRAM, and a full-frame RGB565 sprite (~300KB) does not fit in 320KB of SRAM alongside the Bluetooth stack and SD buffers. Each page has a `drawStatic()` pass (chrome/labels, called once per page change) and a `drawDynamic()` pass (values only, called on a throttled `config::kUiRefreshIntervalMs` cadence) that redraws its own bounded region using TFT_eSPI's background-color text redraw to avoid flicker. There is no slide/fade page-transition animation; page switches redraw immediately.
- **OBDII connection status badge.** The badge in the header displays the connection state with context-sensitive colors: green for `Live` (connected), blue for `Reconnecting`/`ObdConnecting`/`DisplayReady` (in progress), and crimson for `Boot`/`SdInit`/`Stale`/`Degraded` (error/disconnected).
- **RPM warning arc**: drawn as two solid zones, not a gradient, via `gaugewidgets::drawArcGauge`'s `cautionStart`/`dangerStart` parameters — orange (`theme.cautionArc`) from the GAUGES config page's "Shift Light RPM" setting (default 5800) up to its "Redline RPM" setting (default 6200), then red (`theme.dangerArc`) held from Redline RPM out to the end of the arc, so the sweep never drops back to unlit track.
- **Onboard RGB status LED** (`StatusLed::update()` in `src/system/status_led.cpp`): Mirrors the shift/MIL indication with a three-state behavior — flashes the theme's `warningActive` color (same 200 ms cadence as the Page-1 shift-light RPM text flash) whenever RPM is at or above Shift Light RPM, taking priority over a steady-red Check Engine (MIL) indication so the flash is never visually masked; falls back to steady red for MIL alone (when RPM is below Shift Light RPM); off otherwise.
- **Gauge tick marks** (Page 1 RPM/Speed gauges): radial tick marks drawn as two short segments flanking the arc ring — one just outside the outer edge, one just inside the inner edge — at fixed value intervals (RPM: every 500, major every 1000; Speed: every 10, major every 50). Each tick is colored `primaryGaugeArc` once the needle has passed that value and `theme.tickInactiveColor` while still short of it (a field independent of the arc's own `secondaryGaugeArc` track color, so a theme can give ticks a different "unlit" color than the track), providing an at-a-glance "how far past this mark" reference. Ticks are not drawn inside the RPM redline zone (where the arc is already solid red). The outer segment is itself gated by `theme.showOuterTicks` — Mustang S197 sets this false and shows only the inner segment, while the other two themes show both. Toggleable on the Config: UI page via a "GAUGE TICKS" ON/OFF button (default: ON).
- **Round-gauge chrome bezel** (Page 1 RPM/Speed, Page 2 Engine Load, Page 3 Vacuum/Boost): `gaugewidgets::drawGaugeBezel()` draws a decorative ring just outside each round gauge's outer edge, gated by `theme.showGaugeBezel` — on (4px thick, chrome-colored) for Mustang S197, off for the other two themes.
- **Page 6 DTC list shows codes only** (e.g. `P0133`), not human-readable descriptions — no DTC description database is included. A read is triggered automatically whenever Page 6 is opened, plus on-demand via "REFRESH CODES"; "CLEAR CODES" requires a second tap within 5 seconds to confirm.
- **Config pages: UI / GAUGES / USER VARS / LOGS / OBD ADAPTER.** Settings are organized into five focused pages with a shared Save button at the bottom. Each config page displays the Save button (green when any value differs from what's saved on SD, default color when everything matches); tapping it persists all settings across all config pages to `/config.txt` on the SD card and displays "SAVED!" feedback. See [CYD OBD-II SD Card Telemetry Logging Guide](cyd-obd2-sd-logging-guide.md) for the auto-pruning behavior when the card runs low on space. Saving the OBD ADAPTER page also forces an immediate Bluetooth reconnect using the newly saved adapter name/PIN (no reboot needed), unless `secrets/local_config.h` is present, which always takes priority over the on-device selection.
- **Actual source layout** differs from the "Proposed" structure at the bottom of this doc — see [CYD OBD-II Dashboard Implementation Guide](cyd-obd2-dashboard-implementation.md#proposed-source-layout) for the as-built tree.

---

## 🎨 Theme Engine Architecture

The UI supports three distinct visual themes switchable at runtime or persisted via config.

### 1. Mustang OEM S197 Theme
- **Inspiration**: 2005–2010 Ford Mustang S197 instrument cluster, restyled with an LED-backlit gauge face.
- **Color Palette**: Deep midnight navy background (`0x0821`), metallic chrome bezels and needle cap (`0xC618`), vibrant red needle (`0xF800`), LED-green primary text and gauge arc fill (`0x07E0`), soft LED-green secondary text (`0x5D8D`), black unswept arc track (`0x0000`).
- **Tick Marks**: Silver (`0xC618`) before the needle reaches them, switching to LED green once passed; only the inner segment is drawn (the outer segment other themes show is suppressed for this theme).
- **Gauge Bezel**: A decorative 4px-thick chrome ring drawn just outside each round gauge (RPM, Speed, Engine Load, Vacuum/Boost) — not shown on the other two themes.
- **Typography**: Retro-block / racing numerals, tick marks every 500 RPM / 10 MPH.

### 2. Torque Neon Theme
- **Inspiration**: Classic Torque Pro OBD Android App interface.
- **Color Palette**: High-contrast black background (`0x0000`), neon green primary gauge arcs (`0x07E0`), electric cyan secondary accents (`0x07FF`), hot orange warning highlights (`0xFDA0`).
- **Typography**: High-tech digital segments and crisp sans-serif labels.

### 3. Modern Flat Theme
- **Inspiration**: Minimalist modern electric/performance vehicle HUD.
- **Color Palette**: Slate gray background (`0x18C3`), crisp white gauges (`0xFFFF`), accent blue (`0x03FF`), crimson warning badges (`0xD800`).
- **Typography**: Clean vector/bitmap font layout, flat geometric bars, minimal chrome.

### 4. All Themes
- **Redline Arc**: Gradient arc starting at 5500 RPM up to 6200 RPM matching the Mustang 4.6L 3V Modular V8 torque/power drop curve, then held at full redline color to the end of the sweep.

### Implementation Structure (`src/display/theme.h`)

`ThemeId` (`MustangS197 = 0`, `TorqueNeon = 1`, `ModernFlat = 2`) selects a `const ThemeColors&` from `getTheme()`. `ThemeColors` holds every themeable color (background, panel, gauge arc/needle/bezel/text colors, tick colors, warning/highlight/save-button/connection-badge colors) plus a couple of per-theme booleans that toggle whole visual features on or off (`showGaugeBezel`, `showOuterTicks`) and the display name shown on the Config: UI page. Treat `src/display/theme.h` as the authoritative field list rather than a copy here — it has grown several times as themes gained features (most recently the Mustang S197 tick/bezel work), and a duplicated struct in this doc is exactly what goes stale.

---

## ⚡ Display Pipeline & Animation Physics

To achieve flicker-free 30 FPS rendering on the ST7796S (20MHz SPI), use `TFT_eSPI` double-buffered Sprites for active gauges and dynamic screen regions.

> **As-built:** sprites are not used. The ESP32-32E on this board has no PSRAM, and a full-frame RGB565 sprite (480x320x2 bytes, roughly 300 KB) does not fit in 320 KB of SRAM alongside the Bluetooth stack and SD buffers. Widgets instead redraw only their own bounded region each frame, following the two rules below.

### 0. Redraw Rules

Both rules exist because a region painted twice in one frame flickers, and because `drawString()` and `drawSmoothArc()` only repaint the pixels they cover.

- **Variable-width text must clear its own field.** `drawString()` with a background color repaints only the new glyph box, so a readout that loses a character leaves the previous, wider string's outer columns on screen. Draw every value whose rendered width can change through `gaugewidgets::drawFieldText()`, which blanks the margins between the text and its field slot. Readouts drawn over a `fillRoundRect`/`fillRect` painted in the same frame (buttons, badges, the DTC list) are already covered and must not double-clear.
- **Arcs repaint incrementally, but never in stitched pieces.** `drawArcGauge()` carries a `gaugewidgets::ArcGaugeState`; it paints the full track, redline, and fill only on the first draw, after `invalidate()`, or when `maxValue` changes, and otherwise repaints just the value sweep. Each repaint must be issued as **one continuous arc**, because `drawSmoothArc()` anti-aliases every call's ends against the background color and stitching short segments leaves black seam lines. Call `invalidate()` from the owning page's `drawStatic()`, which clears the background behind the gauge.

### 1. Gauge Needle Interpolation & Boot Sweep
Needles use critically damped spring smoothing for realistic weight and zero jitter.

The integration must be sub-stepped. Forward Euler on this spring is only stable while `damping * dt < 2`, and the UI refreshes every `config::kUiRefreshIntervalMs` (100 ms), which with the default `damping = 22` gives 2.2 — the value sign-flips and grows every frame until it reaches NaN, at which point the readout shows garbage and the arc silently stops drawing. Advance in fixed sub-steps instead of one frame-sized step:

```cpp
struct NeedlePhysics {
    float currentValue = 0.0f;
    float velocity     = 0.0f;

    void update(float target, float dtSeconds, float stiffness = 180.0f, float damping = 22.0f) {
        if (dtSeconds <= 0.0f) {
            return;
        }
        constexpr float kMaxStepSeconds = 0.01f;
        constexpr int kMaxSteps = 64;
        int steps = static_cast<int>(dtSeconds / kMaxStepSeconds) + 1;
        if (steps > kMaxSteps) {
            steps = kMaxSteps;
        }
        float step = dtSeconds / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            float accel = (target - currentValue) * stiffness - velocity * damping;
            velocity += accel * step;
            currentValue += velocity * step;
        }
    }
};
```

#### Boot Needle Sweep Sequence
On system boot (after boot animation fade-out), all gauge needles execute a cluster gauge sweep:
1. Interpolate needles from minimum (0%) to maximum (100% scale) in 600ms.
2. Hold at 100% for 150ms.
3. Return smoothly to 0% (or live OBD value) in 500ms.
4. Transition display state to `Live`.

### 2. Page Transition Animations
- **Slide Transition**: When user swipes left/right or taps navigation arrows, draw current page to Sprite A, target page to Sprite B, and slide the offset across X (`0` to `±480` px) over 200ms using ease-out easing.
- **Fade Transition**: Used during boot screen initialization and page switching if low RAM prevents double horizontal buffer allocation. Alpha-blend or step backlight brightness (`TFT_BL` PWM pin 27) smoothly from `0` to `255` over 300ms.

---

## 📱 Touch Navigation & Multi-Page Cluster Architecture

The display is divided into two page groups: **Dashboard pages** (1–4, 6) and **Config pages** (UI, GAUGES, USER VARS, LOGS, OBD ADAPTER). Prev/next navigation cycles only within the active group.

```
Dashboard Group:                        Config Group:
[1] PRIMARY CLUSTER                     [UI] Active Theme
[2] ENGINE LOAD & AIRFLOW               [GAUGES] Shift Light RPM, Redline RPM
[3] CAR-SPECIFIC SENSORS                [USER VARS] Boost Baro Baseline, Warnings, 0-60 Target, HP Factor
[4] MY CAR PERFORMANCE                  [LOGS] Log Interval, Delete Logs
[6] DIAGNOSTICS (DTC)                   [OBD ADAPTER] Adapter Name, Adapter PIN
```

Header layout:
```
+-------------------------------------------------------------------+
|[<]  ●SD  PAGE 1: PRIMARY CLUSTER    [OBDII]  ⚙️  [>]            |
+-------------------------------------------------------------------+
```
- `[<]` — Previous Page (within current group)
- `●SD` — SD logging activity light (green when logging, dark green when inactive; same on every theme)
- `PAGE TITLE` — Name of current page
- `[OBDII]` — Connection status badge (green=Live, blue=Connecting, crimson=Error)
- `⚙️` or `🧭` — Mode toggle button (cog when on dashboard → tap to go to config; steering wheel when on config → tap to go to dashboard)
- `[>]` — Next Page (within current group)

### Navigation Touch Zones
- **Header Touch Bar (Y: 0..40)**:
  - Left region `(X: 0..60)`: Previous Page (within active group; wraps around)
  - Right region `(X: 420..480)`: Next Page (within active group; wraps around)
  - Mode toggle button `(X: 365..420)`: Switch between dashboard and config groups (remembers which page you were on in each group)
- **Check Engine Light (MIL) Touch Zone (X: 310..365, Y: 0..40)**: Tapping the MIL icon when active (or inactive) jumps directly to **Page 6 (Diagnostics)**.
- **Swipe Gestures**: Horizontal swipe left (>60px delta) advances page within group; horizontal swipe right returns within group.

---

## 📄 Cluster Pages Detailed Specification

### Page 1 — Primary Cluster
- **RPM Gauge (Large, Center)**:
  - Dial range: 0 – 7000 RPM.
  - **OEM Mustang Redline Arc**: Exact 4.6L 3V curve with red arc gradient from 5500 RPM to 6200 RPM (`0xF800` to `0x9000`).
  - **Shift Light**: Flashes the RPM gauge bezel bright red/white at configurable threshold (default: 5800 RPM).
  - **Tick Marks**: Radial marks at 500 RPM intervals (major at 1000) flanking the gauge ring, recolored as the needle passes each tick (configurable via Config: UI page).
  - Center digital readout for exact RPM.
- **Speedometer**: Digital + analog scale (0–160 MPH or 0–240 KPH).
  - **Tick Marks**: Radial marks at 10 unit intervals (major at 50) flanking the gauge ring, recolored as the needle passes each tick (configurable via Config: UI page).
- **Coolant Temperature**: Analog/digital gauge with high-temp threshold highlight (>220 °F).
- **Intake Air Temperature (IAT)**: Auxiliary digital display.
- **Throttle Position (TPS)**: Live percentage bar graph (0–100%).
- **Check Engine Light (MIL) Indicator**:
  - Lit bright yellow/orange when ECU reports active Check Engine Light (`PID 01 01` / MIL flag).
  - Acts as a touch zone: Tapping it instantly switches display to **Page 6 (Diagnostics)**.
- **Boot Needle Sweep**: Triggers automatically on initial display transition.

---

### Page 2 — Engine Load & Airflow
Focused on engine intake efficiency and fuel trim diagnostics.
- **Engine Load**: Percentage dial (0–100%).
- **Mass Air Flow (MAF)**: Live grams/sec (g/s) value with peak hold indicator.
- **Timing Advance**: Spark advance in degrees BTDC (-64° to +63.5°).
- **Short Term Fuel Trim (STFT Bank 1)**: Percentage readout (-25% to +25%).
- **Long Term Fuel Trim (LTFT Bank 1)**: Percentage readout (-25% to +25%).

---

### Page 3 — Car-Specific Sensors (Mustang / Ford Focus)
Custom PID calculations for key Ford engine parameters.
- **Battery Voltage**: Measured via OBD PID `01 42` or ELM `ATRVR` command (9.0V – 16.0V range).
- **Fuel Rail Pressure**: Ford specific PID `01 23` (PSI / kPa).
- **MAP & Calculated Vacuum / Boost Gauge**:
  - Uses Manifold Absolute Pressure (`PID 01 0B`) minus Barometric Baseline (configured in the USER VARS config page, default 14.7 PSI / 101.3 kPa).
  - Displays as **Vacuum (inHg)** when MAP < Baro, and **Boost (PSI)** when MAP > Baro. The caption is stacked on two lines (name above unit) so it clears the arc inside the gauge; with no MAP reading it shows `VAC/BOOST` and a blank unit line.
  - Vacuum range: 0 – 30 inHg; Boost range: 0 – 25 PSI.
  - Sits centered at (240, 194) with a 70 px radius, in the gap the two bottom panels leave between x 150 and x 330. Because the range flips between 25 and 30 with the mode, the arc state is re-validated on `maxValue` change rather than assuming a fixed scale.
- **O2 Sensor B1S1**: Upstream Oxygen Sensor Voltage / Lambda (`PID 01 14`).
- **O2 Sensor B2S1**: Upstream Oxygen Sensor Bank 2 Voltage (`PID 01 18`).

---

### Page 4 — 'My Car' Performance & Telemetry
Performance estimation and real-time intake graph.
- **Calculated Horsepower (HP)**:
  - Estimated from MAF sensor airflow:
    $$\text{HP}_{\text{wheel}} \approx \text{MAF (g/s)} \times 0.8$$
- **Calculated Torque (lb-ft)**:
  - Derived from estimated HP and engine RPM:
    $$\text{Torque (lb-ft)} = \frac{\text{HP} \times 5252}{\text{RPM}}$$
  - Clamped when RPM < 500.
- **0–60 MPH Timer**:
  - Automatic start when Speed transitions from 0 to >0 MPH.
  - Timer stops when Speed reaches 60 MPH (displays result in seconds, e.g. `5.42 s`).
  - Reset by tapping timer touch box.
- **Intake Airflow Rolling Graph**:
  - 120-pixel wide line chart showing last 60 seconds of MAF / Throttle readings.

---

### Config Pages — UI / GAUGES / USER VARS / LOGS / OBD ADAPTER

All settings persist across reboots by saving to `/config.txt` on the SD card. A shared **Save** button appears at the bottom of every config page. The button is **green** when any value differs from what's currently saved to SD, and **default color** when all values match what's on disk (dirty-state tracking). Tapping Save writes all settings across all config pages to SD and displays "SAVED!" feedback.

#### Config Page: UI
Settings for display appearance and unit system.

| Setting Field | Options / Range | Default | Description |
| --- | --- | --- | --- |
| **Active Theme** | Modern Flat, Torque Neon, Mustang S197 | Modern Flat | UI visual style; tap-to-cycle button rotates through all three themes |
| **Units** | Standard (MPH/°F/PSI), Metric (KM/H/°C/KPA) | Standard | Display units for speed, temperature, and pressure. Affects all dashboard pages and config field labels/steppers. Does not affect CSV logging (see LOGS page). |
| **Gauge Ticks** | On, Off | On | Radial tick marks on the RPM/Speed gauge scales (every 500/1000 RPM, every 10/50 MPH or KM/H); tap-to-cycle toggle |

#### Config Page: GAUGES
Gauge calibration settings for RPM warning zones.

| Setting Field | Options / Range | Default | Description |
| --- | --- | --- | --- |
| **Shift Light RPM** | 3000 – 6800 RPM (step 100) | 5800 RPM | RPM bezel flash trigger; start of warning arc |
| **Redline RPM** | 5000 – 7000 RPM (step 100) | 6200 RPM | Redline threshold; start of danger (red) arc |
| **Vacuum Gauge Max** | 15 – 30 inHg (step 1) | 30 inHg | Full-scale value for the Page 3 vacuum bar gauge |
| **Boost Gauge Max** | 10 – 40 PSI (step 1) | 25 PSI | Full-scale value for the Page 3 boost bar gauge |

#### Config Page: USER VARS
User-adjustable variables, warning thresholds, and baselines.

| Setting Field | Options / Range | Default | Description |
| --- | --- | --- | --- |
| **Boost Baro Baseline** | 12.0 – 15.5 PSI (step 0.1) | 14.7 PSI | Baseline atmospheric pressure; used to calculate Vacuum/Boost on Page 3 |
| **Coolant Warning Temp** | 180 – 250 °F (step 5) | 220 °F | Coolant temperature above which the Page 1 coolant readout turns the warning color |
| **Low Voltage Warning** | 9.0 – 13.0 V (step 0.1) | 11.5 V | Battery voltage below which the Page 3 voltage readout turns the warning color |
| **0-60 Target Speed** | 40 – 100 MPH (step 5) | 60 MPH | Target speed for the Page 4 acceleration timer |
| **HP Estimate Factor** | 0.5 – 1.2 (step 0.05) | 0.8 | MAF-to-horsepower multiplier used in the Page 4 estimated HP/torque calculation |
| **Fuel Trim Range** | 10 – 50% (step 5) | 25% | +/- range the Page 2 STFT/LTFT bar gauges are scaled to |

#### Config Page: LOGS
SD card logging configuration and management.

| Setting Field | Options / Range | Default | Description |
| --- | --- | --- | --- |
| **Log Interval** | 50ms, 100ms, 250ms, 500ms, 1000ms | 100ms | SD CSV log row write cadence |
| **Log Summary** | (display-only) | - | Live file count and total size of all session logs on SD |
| **Log Units** | Standard (MPH/°F/PSI), Metric (KM/H/°C/KPA) | Standard | CSV logging unit system. **⚠️ CHANGING THIS DELETES ALL LOGS ON SAVE** (shown in red below the toggle) — the toggle stages a pending change; deletion happens when you tap SAVE TO SD, and only if the unit system actually changed since the last save. Toggling back to the original value before saving leaves existing logs untouched. This ensures no CSV file mixes units within its rows. Independent of the display Units setting (you can view the dashboard in one unit system while logging in another). |
| **Delete All Logs** | [ DELETE ALL LOGS ] button | - | Closes active log file, deletes every `mustang_log_*.csv`, opens fresh session. Requires second tap within 5 seconds to confirm. See [CYD OBD-II SD Card Telemetry Logging Guide](cyd-obd2-sd-logging-guide.md) for auto-pruning. |

#### Config Page: OBD ADAPTER
ELM327 Bluetooth adapter identity. Both fields are tap-to-cycle buttons (no on-screen keyboard/text entry exists anywhere in this UI), picking from a fixed list of common ELM327 clone values; an adapter outside these presets still requires the compile-time `secrets/local_config.h` override (see `src/secrets/local_config.example.h`), which always takes priority over this page's selection.

| Setting Field | Options / Range | Default | Description |
| --- | --- | --- | --- |
| **Adapter Name** | OBDII, OBDLink, Vgate, VEEPEAK, OBD2 | OBDII | Bluetooth SPP device name to connect to |
| **Adapter PIN** | 1234, 0000, 1111, 6789 | 1234 | Legacy pairing PIN sent to the adapter |

Unlike other config fields, tapping SAVE TO SD on this page also forces `ObdClient` to immediately disconnect and reconnect using the newly saved name/PIN (no device reboot required) — but only if the values actually changed since the last save.

---

### Page 6 — Diagnostics (DTC Reader)
Read and display OBD-II Diagnostic Trouble Codes.
- **Check Engine Light (MIL) Status**: `MIL ACTIVE (ON)` or `MIL INACTIVE (OFF)`.
- **DTC Decoding**:
  - Requests Mode 03 (`03`) stored trouble codes and Mode 07 (`07`) pending codes from ELM327.
  - Decodes raw 2-byte response into standard OBD-II format:
    - `0x00` -> `P0xxx` (Powertrain)
    - `0x01` -> `C0xxx` (Chassis)
    - `0x02` -> `B0xxx` (Body)
    - `0x03` -> `U0xxx` (Network)
  - Displays formatted code list (e.g. `P0420 - Catalyst System Efficiency Below Threshold`).
- **Clear Codes Touch Zone**: [ CLEAR CODES ] button issues Mode 04 (`04`) command with confirmation prompt.

---

## 🚀 Boot Sequence & Transitions

1. **Power On / Reset**: Initialize TFT display & SPI at 20MHz.
2. **Boot Screen (Build Option `BOOT_IMAGE_MODE`)**:
   - `BOOT_IMAGE_MODE = 0`: Static Ford Mustang pony splash screen with 300ms **fade-in** and 300ms **fade-out**.
   - `BOOT_IMAGE_MODE = 1`: Animated frame-by-frame RGB666 sequence at 20 FPS with fade-in/fade-out transitions.
3. **SD Card & Config Load**: Load saved preferences from SD (`/config.json`).
4. **Gauge Needle Sweep**: Execute 1.2-second full range gauge sweep animation on Page 1.
5. **OBD Connection**: Display connection status badge (`CONNECTING...`) until Bluetooth ELM327 handshake completes, then transition to `LIVE`.

---

## 🛠️ File Structure & Code Responsibilities

Add these files under `src/display/` and `src/system/`:

```text
src/
  display/
    theme.h / theme.cpp             # Color definitions and theme manager
    gauge_widgets.h / .cpp           # Smooth gauge needle, arc rendering, shift light
    cluster_pages.h / .cpp           # Pages 1 to 6 layout and draw functions
    page_transitions.h / .cpp        # Slide/fade animation engine
    touch_handler.h / .cpp           # Touch calibration, gesture detection, tap zones
    boot_player.h / .cpp             # Static/animated boot screen with fade
  system/
    config_store.h / .cpp            # Persistence manager for SD card configuration
    dtc_decoder.h / .cpp             # OBD Mode 03/07 DTC parser (P/C/B/U format)
```

---

## 🧪 Verification Checklist

- [ ] All 6 dashboard pages (1–4, 6) render cleanly at 480×320 landscape resolution.
- [ ] All 5 config pages (UI, GAUGES, USER VARS, LOGS, OBD ADAPTER) render cleanly.
- [ ] OBD ADAPTER page's Adapter Name/PIN tap-to-cycle buttons work, and tapping Save with a changed value forces an immediate Bluetooth reconnect without a reboot.
- [ ] Theme switching (Modern Flat -> Torque Neon -> Mustang S197 -> Modern Flat, via the UI page's Active Theme cycle button) immediately recolors gauges, bezels, needles, and text, and the selection survives Save + reboot.
- [ ] Page 1 RPM gauge correctly displays the orange Shift Light-to-Redline arc and the red Redline-to-max arc, holds the red zone to the end of the sweep, and triggers bezel shift light flash.
- [ ] Shift Light RPM and Redline RPM on GAUGES page control the arc colors correctly.
- [ ] Needles perform a smooth full-scale sweep on boot and update continuously without jitter.
- [ ] Sweeping a gauge up and back down leaves no seam lines in either the fill or the unlit track.
- [ ] Readouts that lose a digit or change caption (RPM, coolant, vacuum/boost, MIL line) leave no leftover characters.
- [ ] Tapping the MIL indicator on Page 1 switches instantly to Page 6.
- [ ] Tapping the mode toggle button (⚙️ or 🧭) in the header switches between the dashboard group and config group, remembering which page was visited in each group.
- [ ] Prev/next navigation cycles only within the active group (dashboard or config); never crosses into the other group.
- [ ] OBDII connection badge displays: green when Live, blue when Reconnecting/ObdConnecting/DisplayReady, crimson for Boot/SdInit/Stale/Degraded.
- [ ] Save button on config pages is green when any value differs from saved, default color when all match saved values.
- [ ] Gauge tick marks on Page 1 (RPM and Speed) render correctly as two short segments flanking the gauge ring (just outside the outer edge and just inside the inner edge), recolor from secondary to primary as the needle passes each tick, and are absent inside the RPM redline zone.
- [ ] Gauge ticks can be toggled ON/OFF from the Config: UI page "GAUGE TICKS" button, and the setting persists after Save + power cycle.
- [ ] Calculated Horsepower, Torque, Vacuum/Boost, and 0-60 timer update correctly on Pages 3 & 4.
- [ ] Config page settings save to SD card and persist after power cycle.
- [ ] Page 6 correctly decodes and displays DTCs in `P0xxx` / `C0xxx` format.
- [ ] Boot screen executes smooth fade-in and fade-out transitions before launching the cluster.
