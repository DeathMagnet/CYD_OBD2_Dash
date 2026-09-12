#include "obd/obd_pids.h"
#include <string.h>
#include <stdlib.h>

const PidDefinition kPidTable[static_cast<uint8_t>(ObdPid::Count)] = {
    {ObdPid::MonitorStatus,        0x01, 4, false, "Monitor Status"},
    {ObdPid::EngineLoad,           0x04, 1, false, "Engine Load"},
    {ObdPid::Coolant,              0x05, 1, false, "Coolant Temp"},
    {ObdPid::ShortTermFuelTrimB1,  0x06, 1, false, "STFT Bank 1"},
    {ObdPid::LongTermFuelTrimB1,   0x07, 1, false, "LTFT Bank 1"},
    {ObdPid::Map,                  0x0B, 1, false, "MAP"},
    {ObdPid::Rpm,                  0x0C, 2, true,  "Engine RPM"},
    {ObdPid::Speed,                0x0D, 1, true,  "Vehicle Speed"},
    {ObdPid::TimingAdvance,        0x0E, 1, false, "Timing Advance"},
    {ObdPid::Iat,                  0x0F, 1, false, "Intake Air Temp"},
    {ObdPid::Maf,                  0x10, 2, false, "MAF"},
    {ObdPid::Throttle,             0x11, 1, false, "Throttle Position"},
    {ObdPid::O2B1S1,               0x14, 2, false, "O2 Bank1 Sensor1"},
    {ObdPid::O2B2S1,               0x18, 2, false, "O2 Bank2 Sensor1"},
    {ObdPid::FuelRailPressure,     0x23, 2, false, "Fuel Rail Pressure"},
    {ObdPid::FuelLevel,            0x2F, 1, false, "Fuel Level"},
    {ObdPid::Baro,                 0x33, 1, false, "Barometric Pressure"},
    {ObdPid::ControlModuleVoltage, 0x42, 2, false, "Control Module Voltage"},
};

const PidDefinition& getPidDefinition(ObdPid id) {
    return kPidTable[static_cast<uint8_t>(id)];
}

namespace {

float celsiusToFahrenheit(float celsiusValue) {
    return celsiusValue * 9.0F / 5.0F + 32.0F;
}

float kphToMph(float kph) {
    return kph * 0.621371F;
}

} // namespace

bool decodePidValue(ObdPid id, const uint8_t* data, uint8_t len, float& outValue) {
    const PidDefinition& def = getPidDefinition(id);
    if (len < def.expectedBytes) {
        return false;
    }

    switch (id) {
        case ObdPid::MonitorStatus:
            // Caller extracts MIL (bit 7) and DTC count (bits 0-6) from this
            // raw status byte; both telemetry fields share this one PID.
            outValue = static_cast<float>(data[0]);
            return true;

        case ObdPid::EngineLoad:
        case ObdPid::Throttle:
        case ObdPid::FuelLevel:
            outValue = data[0] * 100.0F / 255.0F;
            return true;

        case ObdPid::Coolant:
        case ObdPid::Iat:
            outValue = celsiusToFahrenheit(static_cast<float>(data[0]) - 40.0F);
            return true;

        case ObdPid::ShortTermFuelTrimB1:
        case ObdPid::LongTermFuelTrimB1:
            outValue = (static_cast<float>(data[0]) - 128.0F) * 100.0F / 128.0F;
            return true;

        case ObdPid::Map:
        case ObdPid::Baro:
            outValue = static_cast<float>(data[0]);
            return true;

        case ObdPid::Rpm:
            outValue = ((static_cast<uint16_t>(data[0]) * 256U) + data[1]) / 4.0F;
            return true;

        case ObdPid::Speed:
            outValue = kphToMph(static_cast<float>(data[0]));
            return true;

        case ObdPid::TimingAdvance:
            outValue = (static_cast<float>(data[0]) / 2.0F) - 64.0F;
            return true;

        case ObdPid::Maf:
            outValue = ((static_cast<uint16_t>(data[0]) * 256U) + data[1]) / 100.0F;
            return true;

        case ObdPid::O2B1S1:
        case ObdPid::O2B2S1:
            outValue = static_cast<float>(data[0]) / 200.0F;
            return true;

        case ObdPid::FuelRailPressure:
            outValue = ((static_cast<uint16_t>(data[0]) * 256U) + data[1]) * 10.0F;
            return true;

        case ObdPid::ControlModuleVoltage:
            outValue = ((static_cast<uint16_t>(data[0]) * 256U) + data[1]) / 1000.0F;
            return true;

        default:
            return false;
    }
}

bool parseObdResponseLine(const char* line, uint8_t pid, uint8_t* dataOut, uint8_t maxDataBytes, uint8_t& dataLenOut) {
    dataLenOut = 0;
    if (line == nullptr || line[0] == '\0') {
        return false;
    }

    char buf[64];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buf, " \r\n", &savePtr);

    uint8_t bytes[16];
    uint8_t byteCount = 0;
    while (token != nullptr && byteCount < sizeof(bytes)) {
        char* endPtr = nullptr;
        long parsed = strtol(token, &endPtr, 16);
        if (endPtr == token || *endPtr != '\0' || parsed < 0 || parsed > 0xFF) {
            return false; // Non-hex noise: "SEARCHING...", "NO DATA", prompts, etc.
        }
        bytes[byteCount++] = static_cast<uint8_t>(parsed);
        token = strtok_r(nullptr, " \r\n", &savePtr);
    }

    if (byteCount < 2 || bytes[0] != 0x41 || bytes[1] != pid) {
        return false;
    }

    uint8_t remaining = byteCount - 2;
    uint8_t copyLen = remaining < maxDataBytes ? remaining : maxDataBytes;
    for (uint8_t i = 0; i < copyLen; ++i) {
        dataOut[i] = bytes[2 + i];
    }
    dataLenOut = copyLen;
    return true;
}
