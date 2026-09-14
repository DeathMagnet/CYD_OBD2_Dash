#pragma once

#include <stdint.h>

// One polled reading plus the metadata needed to tell a genuine value apart
// from an unsupported/timed-out PID. Never treat valid=false as zero.
struct TelemetryValue {
    float value = 0.0F;
    bool valid = false;
    uint32_t updatedAtMs = 0;

    TelemetryValue() = default;
    TelemetryValue(float initialValue, bool isValid, uint32_t updatedAt)
        : value(initialValue), valid(isValid), updatedAtMs(updatedAt) {}
};

inline bool isFresh(const TelemetryValue& telemetryValue, uint32_t nowMs, uint32_t staleThresholdMs) {
    return telemetryValue.valid && (nowMs - telemetryValue.updatedAtMs) <= staleThresholdMs;
}

// Latest known readings for a 2006 Ford Mustang GT (4.6L 3V), covering every
// PID this project polls. Most fields use the CSV logging schema's Imperial
// units natively (rpm, mph, °F, %, V); the three pressure fields (mapKpa,
// fuelPressureKpa, baroKpa) are stored in kPa and converted to PSI by
// csv_logger.cpp only when writing the non-metric log.
struct TelemetrySnapshot {
    TelemetryValue rpm;              // rpm
    TelemetryValue speedMph;         // mph
    TelemetryValue coolantF;         // deg F
    TelemetryValue throttlePct;      // percent
    TelemetryValue fuelPct;          // percent (fuel tank level)
    TelemetryValue voltageV;         // volts (control module supply)
    TelemetryValue mapKpa;           // kPa (manifold absolute pressure)
    TelemetryValue iatF;             // deg F (intake air temperature)
    TelemetryValue engineLoadPct;    // percent (calculated engine load)
    TelemetryValue mafGps;           // grams/sec
    TelemetryValue timingAdvanceDeg; // degrees BTDC
    TelemetryValue stftPct;          // percent (short term fuel trim, bank 1)
    TelemetryValue ltftPct;          // percent (long term fuel trim, bank 1)
    TelemetryValue fuelPressureKpa;  // kPa (fuel rail pressure)
    TelemetryValue o2B1S1V;          // volts (O2 sensor bank 1 sensor 1)
    TelemetryValue o2B2S1V;          // volts (O2 sensor bank 2 sensor 1)
    TelemetryValue baroKpa;          // kPa (barometric pressure)
    TelemetryValue milOn;            // 0/1 (check engine light state)
    TelemetryValue dtcCount;         // count of stored DTCs reported by monitor status

    bool connected = false;
    uint32_t capturedAtMs = 0;
};
