#include "obd/obd_client.h"
#include <Arduino.h>
#include <string.h>
#include "app_config.h"

#if __has_include("secrets/local_config.h")
#include "secrets/local_config.h"
#define OBD_LOCAL_CONFIG_AVAILABLE 1
#else
#define OBD_LOCAL_CONFIG_AVAILABLE 0
#endif

namespace {

// Secondary PIDs are round-robined one-per-cycle behind the always-polled
// RPM/speed pair so a full sweep completes in a bounded, predictable time
// without overloading the adapter (see docs/cyd-obd2-dashboard-implementation.md).
constexpr ObdPid kSecondaryPids[] = {
    ObdPid::MonitorStatus, ObdPid::EngineLoad, ObdPid::Coolant,
    ObdPid::ShortTermFuelTrimB1, ObdPid::LongTermFuelTrimB1, ObdPid::Map,
    ObdPid::TimingAdvance, ObdPid::Iat, ObdPid::Maf, ObdPid::Throttle,
    ObdPid::O2B1S1, ObdPid::O2B2S1, ObdPid::FuelRailPressure,
    ObdPid::FuelLevel, ObdPid::Baro, ObdPid::ControlModuleVoltage,
};
constexpr size_t kSecondaryPidCount = sizeof(kSecondaryPids) / sizeof(kSecondaryPids[0]);

void decodeAllLinesForDtc(char* mutableBuffer, uint8_t modeAckByte, DtcList& out) {
    char* savePtr = nullptr;
    char* line = strtok_r(mutableBuffer, "\r\n", &savePtr);
    while (line != nullptr) {
        decodeDtcResponseLine(line, modeAckByte, out);
        line = strtok_r(nullptr, "\r\n", &savePtr);
    }
}

} // namespace

bool ObdClient::begin() {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        Serial.println("[OBD] Failed to create mutex; OBD disabled.");
        return false;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        &ObdClient::taskEntry, "obd_task", config::kObdTaskStackWords, this,
        config::kObdTaskPriority, &taskHandle_, config::kObdTaskCore);

    if (created != pdPASS) {
        Serial.println("[OBD] Failed to start background task.");
        return false;
    }

    Serial.println("[OBD] Background task started.");
    return true;
}

void ObdClient::taskEntry(void* param) {
    static_cast<ObdClient*>(param)->taskLoop();
}

void ObdClient::setSnapshotConnected(bool connected) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        snapshot_.connected = connected;
        xSemaphoreGive(mutex_);
    }
}

bool ObdClient::sendCommand(const char* command, char* responseOut, size_t responseCapacity, uint32_t timeoutMs) {
    while (btSerial_.available()) {
        btSerial_.read(); // Drain stale bytes left over from a previous timeout.
    }

    btSerial_.print(command);
    btSerial_.print('\r');

    size_t len = 0;
    uint32_t deadlineMs = millis() + timeoutMs;
    bool sawPrompt = false;

    while (static_cast<int32_t>(millis() - deadlineMs) < 0) {
        if (!btSerial_.connected()) {
            responseOut[len] = '\0';
            return false;
        }
        while (btSerial_.available()) {
            char c = static_cast<char>(btSerial_.read());
            if (c == '>') {
                sawPrompt = true;
                break;
            }
            if (len < responseCapacity - 1) {
                responseOut[len++] = c;
            }
        }
        if (sawPrompt) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    responseOut[len] = '\0';
    return sawPrompt;
}

bool ObdClient::runInitSequence() {
    struct InitCommand {
        const char* command;
        uint32_t timeoutMs;
    };
    const InitCommand initCommands[] = {
        {"ATZ", config::kObdResetCommandTimeoutMs},
        {"ATE0", config::kObdCommandTimeoutMs},
        {"ATL0", config::kObdCommandTimeoutMs},
        {"ATH0", config::kObdCommandTimeoutMs},
        {"ATSP0", config::kObdCommandTimeoutMs},
    };

    char response[64];
    for (const InitCommand& initCommand : initCommands) {
        if (!sendCommand(initCommand.command, response, sizeof(response), initCommand.timeoutMs)) {
            Serial.printf("[OBD] Init command '%s' timed out.\n", initCommand.command);
            return false;
        }
    }

    // Confirm the ECU actually answers mode 01 before declaring Live.
    if (!sendCommand("0100", response, sizeof(response), config::kObdCommandTimeoutMs)) {
        Serial.println("[OBD] ECU did not respond to PID-support probe (0100).");
        return false;
    }

    uint8_t data[8];
    uint8_t dataLen = 0;
    char lineBuf[64];
    strncpy(lineBuf, response, sizeof(lineBuf) - 1);
    lineBuf[sizeof(lineBuf) - 1] = '\0';
    char* savePtr = nullptr;
    char* line = strtok_r(lineBuf, "\r\n", &savePtr);
    while (line != nullptr) {
        if (parseObdResponseLine(line, 0x00, data, sizeof(data), dataLen)) {
            return true;
        }
        line = strtok_r(nullptr, "\r\n", &savePtr);
    }

    Serial.println("[OBD] PID-support probe returned no valid mode 01 ack.");
    return false;
}

void ObdClient::pollPid(ObdPid id, uint32_t nowMs) {
    const PidDefinition& def = getPidDefinition(id);
    char command[8];
    snprintf(command, sizeof(command), "01%02X", def.pid);

    char response[80];
    if (!sendCommand(command, response, sizeof(response), config::kObdCommandTimeoutMs)) {
        consecutiveFailures_++;
        return;
    }

    char lineBuf[80];
    strncpy(lineBuf, response, sizeof(lineBuf) - 1);
    lineBuf[sizeof(lineBuf) - 1] = '\0';

    uint8_t data[8];
    uint8_t dataLen = 0;
    bool parsed = false;
    char* savePtr = nullptr;
    char* line = strtok_r(lineBuf, "\r\n", &savePtr);
    while (line != nullptr) {
        if (parseObdResponseLine(line, def.pid, data, sizeof(data), dataLen)) {
            parsed = true;
            break;
        }
        line = strtok_r(nullptr, "\r\n", &savePtr);
    }

    float value = 0.0F;
    if (!parsed || !decodePidValue(id, data, dataLen, value)) {
        consecutiveFailures_++;
        return;
    }

    consecutiveFailures_ = 0;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    switch (id) {
        case ObdPid::MonitorStatus: {
            uint8_t statusByte = static_cast<uint8_t>(value);
            snapshot_.milOn = {(statusByte & 0x80U) ? 1.0F : 0.0F, true, nowMs};
            snapshot_.dtcCount = {static_cast<float>(statusByte & 0x7FU), true, nowMs};
            break;
        }
        case ObdPid::EngineLoad: snapshot_.engineLoadPct = {value, true, nowMs}; break;
        case ObdPid::Coolant: snapshot_.coolantF = {value, true, nowMs}; break;
        case ObdPid::ShortTermFuelTrimB1: snapshot_.stftPct = {value, true, nowMs}; break;
        case ObdPid::LongTermFuelTrimB1: snapshot_.ltftPct = {value, true, nowMs}; break;
        case ObdPid::Map: snapshot_.mapKpa = {value, true, nowMs}; break;
        case ObdPid::Rpm: snapshot_.rpm = {value, true, nowMs}; break;
        case ObdPid::Speed: snapshot_.speedMph = {value, true, nowMs}; break;
        case ObdPid::TimingAdvance: snapshot_.timingAdvanceDeg = {value, true, nowMs}; break;
        case ObdPid::Iat: snapshot_.iatF = {value, true, nowMs}; break;
        case ObdPid::Maf: snapshot_.mafGps = {value, true, nowMs}; break;
        case ObdPid::Throttle: snapshot_.throttlePct = {value, true, nowMs}; break;
        case ObdPid::O2B1S1: snapshot_.o2B1S1V = {value, true, nowMs}; break;
        case ObdPid::O2B2S1: snapshot_.o2B2S1V = {value, true, nowMs}; break;
        case ObdPid::FuelRailPressure: snapshot_.fuelPressureKpa = {value, true, nowMs}; break;
        case ObdPid::FuelLevel: snapshot_.fuelPct = {value, true, nowMs}; break;
        case ObdPid::Baro: snapshot_.baroKpa = {value, true, nowMs}; break;
        case ObdPid::ControlModuleVoltage: snapshot_.voltageV = {value, true, nowMs}; break;
        default: break;
    }
    snapshot_.connected = true;
    snapshot_.capturedAtMs = nowMs;
    xSemaphoreGive(mutex_);
}

void ObdClient::performDtcRead() {
    DtcList result;

    char storedResponse[160];
    if (sendCommand("03", storedResponse, sizeof(storedResponse), config::kObdCommandTimeoutMs)) {
        decodeAllLinesForDtc(storedResponse, 0x43, result);
    }

    char pendingResponse[160];
    if (sendCommand("07", pendingResponse, sizeof(pendingResponse), config::kObdCommandTimeoutMs)) {
        decodeAllLinesForDtc(pendingResponse, 0x47, result);
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        dtcResult_ = result;
        xSemaphoreGive(mutex_);
    }
    dtcResultReady_.store(true);
    Serial.printf("[OBD] DTC read complete: %u code(s).\n", result.count);
}

void ObdClient::performClearCodes() {
    char response[32];
    if (sendCommand("04", response, sizeof(response), config::kObdCommandTimeoutMs)) {
        Serial.println("[OBD] Clear codes command acknowledged.");
    } else {
        Serial.println("[OBD] Clear codes command timed out.");
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        dtcResult_ = DtcList();
        xSemaphoreGive(mutex_);
    }
    dtcResultReady_.store(false); // Force a fresh read next time Page 6 asks.
}

void ObdClient::getSnapshot(TelemetrySnapshot& out) const {
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        out = snapshot_;
        xSemaphoreGive(mutex_);
    }
}

void ObdClient::getDtcList(DtcList& out) const {
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        out = dtcResult_;
        xSemaphoreGive(mutex_);
    }
}

void ObdClient::taskLoop() {
#if OBD_LOCAL_CONFIG_AVAILABLE
    const char* adapterName = localconfig::kObdAdapterName;
    const char* adapterPin = localconfig::kObdAdapterPin;
#else
    const char* adapterName = config::kObdDefaultAdapterName;
    const char* adapterPin = config::kObdDefaultAdapterPin;
#endif

    btSerial_.begin("CYD_OBD_Dash", true); // Local device name; master role.
    btSerial_.setPin(adapterPin);

    bool initialized = false;

    for (;;) {
        if (!btSerial_.connected()) {
            initialized = false;
            connectionState_.store(ConnectionState::ObdConnecting);
            setSnapshotConnected(false);

            Serial.printf("[OBD] Connecting to '%s'...\n", adapterName);
            if (!btSerial_.connect(String(adapterName))) {
                Serial.println("[OBD] Connect failed; backing off.");
                connectionState_.store(ConnectionState::Reconnecting);
                reconnectBackoffMs_ = (reconnectBackoffMs_ == 0)
                    ? config::kObdReconnectBackoffMs
                    : (reconnectBackoffMs_ * 2);
                if (reconnectBackoffMs_ > config::kObdMaxReconnectBackoffMs) {
                    reconnectBackoffMs_ = config::kObdMaxReconnectBackoffMs;
                }
                vTaskDelay(pdMS_TO_TICKS(reconnectBackoffMs_));
                continue;
            }
            reconnectBackoffMs_ = 0;
            Serial.println("[OBD] Bluetooth link established.");
        }

        if (!initialized) {
            initialized = runInitSequence();
            if (!initialized) {
                Serial.println("[OBD] ELM327 init sequence failed; disconnecting.");
                btSerial_.disconnect();
                connectionState_.store(ConnectionState::Reconnecting);
                setSnapshotConnected(false);
                vTaskDelay(pdMS_TO_TICKS(config::kObdReconnectBackoffMs));
                continue;
            }
            consecutiveFailures_ = 0;
            connectionState_.store(ConnectionState::Live);
            Serial.println("[OBD] ELM327 initialized; polling PIDs.");
        }

        uint32_t nowMs = millis();

        pollPid(ObdPid::Rpm, nowMs);
        pollPid(ObdPid::Speed, nowMs);

        pollPid(kSecondaryPids[secondaryPollIndex_], nowMs);
        secondaryPollIndex_ = static_cast<uint8_t>((secondaryPollIndex_ + 1) % kSecondaryPidCount);

        if (dtcReadRequested_.load()) {
            performDtcRead();
            dtcReadRequested_.store(false);
        }
        if (clearCodesRequested_.load()) {
            performClearCodes();
            clearCodesRequested_.store(false);
        }

        if (consecutiveFailures_ >= config::kObdConsecutiveFailuresForDisconnect) {
            Serial.println("[OBD] Too many consecutive failures; forcing reconnect.");
            btSerial_.disconnect();
            initialized = false;
            connectionState_.store(ConnectionState::Reconnecting);
            setSnapshotConnected(false);
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(config::kObdSecondaryPidIntervalMs));
    }
}
