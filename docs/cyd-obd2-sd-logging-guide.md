# CYD OBD-II SD Card Telemetry Logging Guide

## Overview

This guide details the implementation of timestamped CSV telemetry logging to an SD card on the Hosyond 4.0-inch ESP32-32E CYD OBD-II Dashboard.

When an SD card is present and `SD_LOGGING_ENABLED` is defined, the system automatically creates a new session log file on boot, continuously buffers high-frequency telemetry rows, and flushes them to flash on a non-blocking interval.

---

## ✅ Implementation Status

The class that ships in `src/logging/csv_logger.h/.cpp` is named `CsvLogger` (not `SdLogger` as sketched below) and extends the design in three ways:

- **Row interval is user-configurable at runtime**, not a fixed `SD_LOG_ROW_INTERVAL_MS` constant. The LOGS config page cycles through `config::kLogRowIntervalOptionsMs` (50/100/250/500/1000 ms), persisted via `ConfigStore` to `/config.txt`; `main.cpp` passes `configStore.settings().logIntervalMs` into `CsvLogger::update()` every call.
- **Automatic capacity pruning.** Before opening a new session file, and again on every flush cycle, `CsvLogger::enforceCapacity()` compares `SD.totalBytes() - SD.usedBytes()` against `config::kSdMinFreeBytes` (5 MB). If free space is short, it repeatedly deletes the single oldest `obd_log_*.csv` (by parsed session index, never the file currently being written) until there's headroom again or no prunable file remains.
- **Log management on LOGS config page.** `CsvLogger::getLogSummary()` scans the SD root for a file count and total byte size, rendered on the LOGS config page alongside a "DELETE ALL LOGS" button (`CsvLogger::deleteAllLogs()`, tap-to-confirm within 5 seconds) that closes the active file, removes every session log, and immediately opens a fresh one — but only resumes writing if logging was actually active beforehand, so tapping it with `SD_LOGGING_ENABLED` unset can't silently start a session.

The 20-column schema, NVS session-index scheme, and buffered-flush strategy below are otherwise implemented as designed.

---

## 📁 File Naming & NVS Session Persistence

### Auto-Incrementing Session Index

Log files are stored at the root directory of the SD card (`/SD/` or card root `/`) using a 3-digit zero-padded session index:

```text
/SD/obd_log_001.csv
/SD/obd_log_002.csv
/SD/obd_log_003.csv
...
/SD/obd_log_999.csv
```

### NVS (Non-Volatile Storage) Key

To ensure unique, monotonically increasing log file names across restarts, the session counter is stored in ESP32 NVS using the `Preferences` library under namespace `"obd_dash"`:

```cpp
#include <Preferences.h>
#include <SD.h>

uint32_t getNextSessionIndex() {
    Preferences prefs;
    prefs.begin("obd_dash", false); // Open in read-write mode
    uint32_t lastIndex = prefs.getUInt("session_idx", 0);
    uint32_t nextIndex = lastIndex + 1;
    prefs.putUInt("session_idx", nextIndex);
    prefs.end();
    return nextIndex;
}

void buildLogFilePath(char* buffer, size_t bufferSize, uint32_t sessionIdx) {
    snprintf(buffer, bufferSize, "/obd_log_%03u.csv", static_cast<unsigned int>(sessionIdx));
}
```

---

## 📊 CSV Column Schema & Types

Every log file begins with a single header row defining the exact 20-column schema. The schema depends on the **Log Units** setting configured on the LOGS config page:

### Standard Units (MPH/°F/PSI)
```csv
timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,voltage_v,map_psi,iat_f,engine_load_pct,maf_gps,timing_advance_deg,stft_pct,ltft_pct,fuel_pressure_psi,o2_b1s1_v,o2_b2s1_v,baro_psi,cel_on,dtc_count
```

### Metric Units (KM/H/°C/KPA)
```csv
timestamp_ms,rpm,speed_kph,coolant_c,throttle_pct,fuel_pct,voltage_v,map_kpa,iat_c,engine_load_pct,maf_gps,timing_advance_deg,stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count
```

**⚠️ Note**: Changing the Log Units setting on the LOGS config page stages a pending change; **deletion happens when the change is saved** (SAVE TO SD button), and **only if the unit system actually changed since the last save**. Toggling back to the original value before saving leaves existing logs untouched. This ensures no CSV file ever mixes unit systems within its rows.

### Field Definitions

| Column | Type | Format | Description | Unit Dependent? |
| --- | --- | --- | --- | --- |
| `timestamp_ms` | Integer | `%lu` | Uptime in milliseconds since device boot (`millis()`) | No |
| `rpm` | Integer | `%d` | Engine RPM (0 – 12,000) | No |
| `speed_mph` / `speed_kph` | Integer | `%d` | Vehicle speed in **MPH** (standard) or **km/h** (metric) | ✓ Yes |
| `coolant_f` / `coolant_c` | Integer | `%d` | Engine coolant temperature in **°F** (standard) or **°C** (metric) | ✓ Yes |
| `throttle_pct` | Integer | `%d` | Throttle position percentage (0–100%) | No |
| `fuel_pct` | Integer | `%d` | Fuel tank level percentage (0–100%) | No |
| `voltage_v` | Float | `%.2f` | Control module supply voltage (e.g. `14.20`) | No |
| `map_psi` / `map_kpa` | Integer | `%d` | Manifold Absolute Pressure in **PSI** (standard) or **kPa** (metric) | ✓ Yes |
| `iat_f` / `iat_c` | Integer | `%d` | Intake air temperature in **°F** (standard) or **°C** (metric) | ✓ Yes |
| `engine_load_pct` | Integer | `%d` | Calculated engine load percentage (0–100%) | No |
| `maf_gps` | Float | `%.2f` | Mass Air Flow sensor reading in grams/second | No |
| `timing_advance_deg` | Integer | `%d` | Ignition timing advance in degrees BTDC (-64 to +63) | No |
| `stft_pct` | Integer | `%d` | Short-Term Fuel Trim Bank 1 (-25% to +25%) | No |
| `ltft_pct` | Integer | `%d` | Long-Term Fuel Trim Bank 1 (-25% to +25%) | No |
| `fuel_pressure_psi` / `fuel_pressure_kpa` | Integer | `%d` | Fuel rail pressure in **PSI** (standard) or **kPa** (metric) | ✓ Yes |
| `o2_b1s1_v` | Float | `%.2f` | Oxygen sensor Bank 1 Sensor 1 voltage (0.00 – 1.27V) | No |
| `o2_b2s1_v` | Float | `%.2f` | Oxygen sensor Bank 2 Sensor 1 voltage (0.00 – 1.27V) | No |
| `baro_psi` / `baro_kpa` | Integer | `%d` | Barometric pressure in **PSI** (standard) or **kPa** (metric) | ✓ Yes |
| `cel_on` | Integer | `%d` | Check Engine Light (MIL) status: `1` = active, `0` = inactive | No |
| `dtc_count` | Integer | `%d` | Count of stored Diagnostic Trouble Codes at log time | No |

### Sample CSV Log Output

#### Standard Units (MPH/°F/PSI):
```csv
timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,voltage_v,map_psi,iat_f,engine_load_pct,maf_gps,timing_advance_deg,stft_pct,ltft_pct,fuel_pressure_psi,o2_b1s1_v,o2_b2s1_v,baro_psi,cel_on,dtc_count
0,0,0,72,0,87,12.40,14,68,12,2.10,10,-1,0,55,0.42,0.38,14,0,0
103,820,12,73,2,87,12.40,14,68,14,2.40,11,-1,0,55,0.44,0.40,14,0,0
207,1240,32,78,18,87,12.30,15,69,22,3.80,13,0,0,56,0.51,0.47,14,1,2
```

#### Metric Units (KM/H/°C/KPA):
```csv
timestamp_ms,rpm,speed_kph,coolant_c,throttle_pct,fuel_pct,voltage_v,map_kpa,iat_c,engine_load_pct,maf_gps,timing_advance_deg,stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count
0,0,0,22,0,87,12.40,101,20,12,2.10,10,-1,0,380,0.42,0.38,101,0,0
103,820,19,23,2,87,12.40,101,20,14,2.40,11,-1,0,382,0.44,0.40,101,0,0
207,1240,51,26,18,87,12.30,108,21,22,3.80,13,0,0,386,0.51,0.47,101,1,2
```

Note: The Log Units setting controls which unit system is logged. Switching between Standard and Metric modes **deletes all existing logs** and begins a fresh session, ensuring consistency within each log file.

---

## ⚡ Buffering & Non-Blocking Flush Strategy

Writing every telemetry row directly to physical SD flash creates high SPI latency spikes (up to 50–100ms per flush), causing visual needle stutter on the gauge display.

### Buffering Design
1. **Row Appends**: Each telemetry snapshot formats a CSV line into a preallocated ring buffer or string stream.
2. **Periodic Flush**: Physical `file.flush()` is called only when `SD_FLUSH_INTERVAL_MS` elapses (default **500ms**).
3. **Safe Shutdown**: On controlled reboot or SD ejection request, the log file is explicitly flushed and closed.

### Configuration (`include/config.h` / `src/app_config.h`)

```cpp
#pragma once
#include <stdint.h>

// SD Logging Configuration
constexpr uint32_t SD_LOG_ROW_INTERVAL_MS = 100;  // Write a row every 100ms (10 Hz)
constexpr uint32_t SD_FLUSH_INTERVAL_MS   = 500;  // Physical flash sync every 500ms
```

---

## ⚙️ Conditional Compilation (`SD_LOGGING_ENABLED`)

SD Card logging is completely optional. For bench-testing without an SD card inserted, comment out `-D SD_LOGGING_ENABLED` in the shared `[env]` section of `platformio.ini`:

```ini
[env]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
    -D USER_SETUP_LOADED
    -D ST7796_DRIVER
    ; -D SD_LOGGING_ENABLED   ; Comment out to compile without SD logging calls
```

Both `cyd_4inch` and `cyd_4inch_sim` inherit this flag. Logging is deliberately **orthogonal** to `OBD_SIMULATION_ENABLED`: a simulated build logs to SD exactly like a live one, which makes the scripted drive cycle a dense, repeatable exercise of the writer. Never guard logging code on the simulation flag.

### Module Structure (`src/logging/csv_logger.h` & `.cpp`)

The as-built `CsvLogger` class differs from the sketch above (which showed a fixed row-interval constant and no capacity pruning):

- **Class name**: `CsvLogger` (not `SdLogger`)
- **Row interval**: Runtime-configurable per `update()` call; passed in via `rowIntervalMs` parameter (a user-configurable value from the Logs config page, cycling through 50/100/250/500/1000 ms)
- **Capacity pruning**: `enforceCapacity()` is called before opening a new session and on every flush; it deletes the oldest `obd_log_*.csv` if free space drops below `config::kSdMinFreeBytes` (5 MB)
- **Log management**: `getLogSummary()` scans for file count and total bytes; `deleteAllLogs()` closes the active file, removes every session log, and opens a fresh one (with two-tap confirm on the LOGS config page)
- **CSV header**: Dynamic; picks one of two headers based on `useMetricLogs_` (standard or metric unit schema); user can toggle via the Logs config page, but toggling **deletes all existing logs when saved** to prevent unit-mixing within rows

For a complete API reference, see [`src/logging/csv_logger.h`](../src/logging/csv_logger.h).

---

## 🧪 Verification Checklist

- [ ] Firmware builds cleanly with `-D SD_LOGGING_ENABLED` active in `platformio.ini`.
- [ ] Firmware builds cleanly with `; -D SD_LOGGING_ENABLED` commented out (zero SD dependencies linked).
- [ ] On boot with SD card inserted, `/obd_log_001.csv` is created with the exact 20-column header (Standard units by default).
- [ ] Rebooting increments NVS index and creates `/obd_log_002.csv`.
- [ ] Removing SD card during operation degrades state gracefully to `SD OFFLINE` without crashing the display loop.
- [ ] Display needle updates remain at 30 FPS without stutter during periodic 500ms SD flushes.
- [ ] Inspected CSV file on host PC contains valid, uncorrupted, comma-separated numeric rows.
- [ ] On LOGS config page, tapping "LOG UNITS" to toggle the setting does not immediately delete any log files (button label updates, but Save button turns green to show pending change).
- [ ] Tapping "LOG UNITS" twice (e.g. Standard → Metric → Standard) before hitting Save leaves all log files intact, since the net setting didn't actually change.
- [ ] After toggling Log Units to a new value and tapping SAVE TO SD, the previous session file closes, all existing `obd_log_*.csv` files are deleted, and a fresh session file opens with the new unit schema (metric column names: `speed_kph`, `coolant_c`, `map_kpa`, `fuel_pressure_kpa`, `baro_kpa`).
- [ ] Log Units setting is independent of the display Units setting (on UI config page) — you can view the dashboard in one unit system while logging in another.
