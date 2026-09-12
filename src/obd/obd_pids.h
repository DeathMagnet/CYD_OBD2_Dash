#pragma once

#include <stdint.h>
#include <stddef.h>

// Mode 01 PIDs polled for a 2006 Ford Mustang GT (4.6L 3V). This is the full
// set of standard PIDs the project's SD logging schema and UI cluster guide
// call for; an unsupported PID on a given ECU simply reports valid=false.
enum class ObdPid : uint8_t {
    MonitorStatus = 0,    // 01 01 - MIL state + stored DTC count
    EngineLoad,           // 01 04 - calculated engine load
    Coolant,              // 01 05 - coolant temperature
    ShortTermFuelTrimB1,  // 01 06
    LongTermFuelTrimB1,   // 01 07
    Map,                  // 01 0B - manifold absolute pressure
    Rpm,                  // 01 0C
    Speed,                // 01 0D
    TimingAdvance,        // 01 0E
    Iat,                  // 01 0F - intake air temperature
    Maf,                  // 01 10 - mass air flow
    Throttle,             // 01 11 - throttle position
    O2B1S1,               // 01 14 - O2 sensor bank 1 sensor 1 voltage
    O2B2S1,               // 01 18 - O2 sensor bank 2 sensor 1 voltage
    FuelRailPressure,     // 01 23 - Ford fuel rail pressure
    FuelLevel,            // 01 2F - fuel tank level
    Baro,                 // 01 33 - barometric pressure
    ControlModuleVoltage, // 01 42
    Count
};

struct PidDefinition {
    ObdPid id;
    uint8_t pid;           // Raw PID byte (mode 01 always assumed)
    uint8_t expectedBytes; // Data bytes expected in a valid response
    bool highPriority;     // Polled every cycle instead of round-robin
    const char* name;      // Diagnostic label for serial logs
};

// Ordered to match ObdPid's enum values so kPidTable[static_cast<uint8_t>(id)] works.
extern const PidDefinition kPidTable[static_cast<uint8_t>(ObdPid::Count)];

const PidDefinition& getPidDefinition(ObdPid id);

// Decodes the data bytes of a mode 01 response into the field's native unit
// (see telemetry.h for units). Returns false if len does not match what the
// PID requires; callers should treat that as an invalid/unsupported reading.
bool decodePidValue(ObdPid id, const uint8_t* data, uint8_t len, float& outValue);

// Parses one ELM327 response line of hex byte pairs (e.g. "41 0C 1A F8") for
// mode 0x41 acknowledging `pid`. On success, fills dataOut with the bytes
// after the mode/pid echo and returns true. Handles ELM327 noise such as
// "SEARCHING...", ">" prompts, and empty/blank lines by returning false.
bool parseObdResponseLine(const char* line, uint8_t pid, uint8_t* dataOut, uint8_t maxDataBytes, uint8_t& dataLenOut);
