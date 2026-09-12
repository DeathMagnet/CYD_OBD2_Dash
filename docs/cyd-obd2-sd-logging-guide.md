# CYD OBD-II SD Card Telemetry Logging Guide

## Overview

This guide details the implementation of timestamped CSV telemetry logging to an SD card on the Hosyond 4.0-inch ESP32-32E CYD OBD-II Dashboard.

When an SD card is present and `SD_LOGGING_ENABLED` is defined, the system automatically creates a new session log file on boot, continuously buffers high-frequency telemetry rows, and flushes them to flash on a non-blocking interval.

---

## 📁 File Naming & NVS Session Persistence

### Auto-Incrementing Session Index

Log files are stored at the root directory of the SD card (`/SD/` or card root `/`) using a 3-digit zero-padded session index:

```text
/SD/mustang_log_001.csv
/SD/mustang_log_002.csv
/SD/mustang_log_003.csv
...
/SD/mustang_log_999.csv
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
    snprintf(buffer, bufferSize, "/mustang_log_%03u.csv", static_cast<unsigned int>(sessionIdx));
}
```

---

## 📊 CSV Column Schema & Types

Every log file begins with a single header row defining the exact 20-column schema:

```csv
timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,voltage_v,map_kpa,iat_f,engine_load_pct,maf_gps,timing_advance_deg,stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count
```

### Field Definitions

| Column | Type | Format | Description |
| --- | --- | --- | --- |
| `timestamp_ms` | Integer | `%lu` | Uptime in milliseconds since device boot (`millis()`) |
| `rpm` | Integer | `%d` | Engine RPM (0 – 12,000) |
| `speed_mph` | Integer | `%d` | Vehicle speed in MPH (or km/h if metric enabled) |
| `coolant_f` | Integer | `%d` | Engine coolant temperature in °F (or °C if metric) |
| `throttle_pct` | Integer | `%d` | Throttle position percentage (0–100%) |
| `fuel_pct` | Integer | `%d` | Fuel tank level percentage (0–100%) |
| `voltage_v` | Float | `%.2f` | Control module supply voltage (e.g. `14.20`) |
| `map_kpa` | Integer | `%d` | Manifold Absolute Pressure in kPa |
| `iat_f` | Integer | `%d` | Intake air temperature in °F |
| `engine_load_pct` | Integer | `%d` | Calculated engine load percentage (0–100%) |
| `maf_gps` | Float | `%.2f` | Mass Air Flow sensor reading in grams/second |
| `timing_advance_deg` | Integer | `%d` | Ignition timing advance in degrees BTDC (-64 to +63) |
| `stft_pct` | Integer | `%d` | Short-Term Fuel Trim Bank 1 (-25% to +25%) |
| `ltft_pct` | Integer | `%d` | Long-Term Fuel Trim Bank 1 (-25% to +25%) |
| `fuel_pressure_kpa` | Integer | `%d` | Fuel rail pressure in kPa (standard OBD PID 0x0A) |
| `o2_b1s1_v` | Float | `%.2f` | Oxygen sensor Bank 1 Sensor 1 voltage (0.00 – 1.27V) |
| `o2_b2s1_v` | Float | `%.2f` | Oxygen sensor Bank 2 Sensor 1 voltage (0.00 – 1.27V) |
| `baro_kpa` | Integer | `%d` | Barometric pressure in kPa (used for vacuum/boost) |
| `cel_on` | Integer | `%d` | Check Engine Light (MIL) status: `1` = active, `0` = inactive |
| `dtc_count` | Integer | `%d` | Count of stored Diagnostic Trouble Codes at log time |

### Sample CSV Log Output

```csv
timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,voltage_v,map_kpa,iat_f,engine_load_pct,maf_gps,timing_advance_deg,stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count
0,0,0,72,0,87,12.40,101,68,12,2.10,10,-1,0,380,0.42,0.38,101,0,0
103,820,0,73,2,87,12.40,101,68,14,2.40,11,-1,0,382,0.44,0.40,101,0,0
207,1240,0,78,18,87,12.30,108,69,22,3.80,13,0,0,386,0.51,0.47,101,1,2
```

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

SD Card logging is completely optional. For bench-testing without an SD card inserted, comment out `-D SD_LOGGING_ENABLED` in `platformio.ini`:

```ini
[env:cyd_4inch]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
    -D USER_SETUP_LOADED
    -D ST7796_DRIVER
    ; -D SD_LOGGING_ENABLED   ; Comment out to compile without SD logging calls
```

### Module Structure with Preprocessor Wrappers (`src/logging/sd_logger.h` & `sd_logger.cpp`)

```cpp
#pragma once
#include <stdint.h>

struct TelemetrySnapshot;

class SdLogger {
public:
    bool begin();
    void logTelemetry(const TelemetrySnapshot& data, uint32_t nowMs);
    void flush();
    void end();
    bool isLoggingActive() const { return loggingActive_; }

private:
    bool loggingActive_ = false;
    uint32_t lastFlushMs_ = 0;
    uint32_t lastRowMs_ = 0;
};
```

In `src/logging/sd_logger.cpp`:

```cpp
#include "logging/sd_logger.h"

#ifdef SD_LOGGING_ENABLED
#include <SD.h>
#include <SPI.h>
#include <Preferences.h>
#include <stdio.h>

static File logFile;

bool SdLogger::begin() {
    // Initialize SPI and SD card (VSPI: CS=5, MOSI=23, MISO=19, CLK=18)
    if (!SD.begin(5 /* SD CS Pin - GPIO 5 */)) {
        Serial.println("[SD] Mount failed or card not present");
        loggingActive_ = false;
        return false;
    }

    Preferences prefs;
    prefs.begin("obd_dash", false);
    uint32_t sessionIdx = prefs.getUInt("session_idx", 0) + 1;
    prefs.putUInt("session_idx", sessionIdx);
    prefs.end();

    char path[32];
    snprintf(path, sizeof(path), "/mustang_log_%03u.csv", static_cast<unsigned int>(sessionIdx));

    logFile = SD.open(path, FILE_WRITE);
    if (!logFile) {
        Serial.printf("[SD] Failed to open %s for writing\n", path);
        loggingActive_ = false;
        return false;
    }

    // Write CSV Header
    logFile.println("timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,"
                    "voltage_v,map_kpa,iat_f,engine_load_pct,maf_gps,timing_advance_deg,"
                    "stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count");
    logFile.flush();

    loggingActive_ = true;
    Serial.printf("[SD] Logging initialized: %s\n", path);
    return true;
}

void SdLogger::logTelemetry(const TelemetrySnapshot& d, uint32_t nowMs) {
    if (!loggingActive_ || !logFile) return;

    if (nowMs - lastRowMs_ >= SD_LOG_ROW_INTERVAL_MS) {
        lastRowMs_ = nowMs;
        logFile.printf("%lu,%d,%d,%d,%d,%d,%.2f,%d,%d,%d,%.2f,%d,%d,%d,%d,%.2f,%.2f,%d,%d,%d\n",
            nowMs, d.rpm, d.speedMph, d.coolantF, d.throttlePct, d.fuelPct,
            d.voltageV, d.mapKpa, d.iatF, d.engineLoadPct, d.mafGps, d.timingAdvanceDeg,
            d.stftPct, d.ltftPct, d.fuelPressureKpa, d.o2B1S1V, d.o2B2S1V, d.baroKpa,
            d.celOn ? 1 : 0, d.dtcCount);
    }

    if (nowMs - lastFlushMs_ >= SD_FLUSH_INTERVAL_MS) {
        lastFlushMs_ = nowMs;
        logFile.flush();
    }
}

void SdLogger::flush() {
    if (loggingActive_ && logFile) logFile.flush();
}

void SdLogger::end() {
    if (logFile) {
        logFile.flush();
        logFile.close();
    }
    loggingActive_ = false;
}

#else // !SD_LOGGING_ENABLED Stub implementations for zero overhead

bool SdLogger::begin() { return false; }
void SdLogger::logTelemetry(const TelemetrySnapshot&, uint32_t) {}
void SdLogger::flush() {}
void SdLogger::end() {}

#endif
```

---

## 🧪 Verification Checklist

- [ ] Firmware builds cleanly with `-D SD_LOGGING_ENABLED` active in `platformio.ini`.
- [ ] Firmware builds cleanly with `; -D SD_LOGGING_ENABLED` commented out (zero SD dependencies linked).
- [ ] On boot with SD card inserted, `/mustang_log_001.csv` is created with the exact 20-column header.
- [ ] Rebooting increments NVS index and creates `/mustang_log_002.csv`.
- [ ] Removing SD card during operation degrades state gracefully to `SD OFFLINE` without crashing the display loop.
- [ ] Display needle updates remain at 30 FPS without stutter during periodic 500ms SD flushes.
- [ ] Inspected CSV file on host PC contains valid, uncorrupted, comma-separated numeric rows.
