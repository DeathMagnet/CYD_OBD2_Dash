#pragma once

#include <stdint.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Bench/demo builds replace the Bluetooth transport with a scripted data source.
#ifndef OBD_SIMULATION_ENABLED
#define OBD_SIMULATION_ENABLED 0
#endif

#if OBD_SIMULATION_ENABLED
#include "obd/obd_simulator.h"
#else
#include <BluetoothSerial.h>
#endif

#include "obd/telemetry.h"
#include "obd/obd_pids.h"
#include "system/connection_state.h"
#include "system/dtc_decoder.h"

// Owns the Bluetooth Classic SPP link to an ELM327 adapter, AT/PID command
// sequencing, and response parsing.
//
// BluetoothSerial::connect() and every command round-trip in this class are
// blocking calls with multi-second worst cases (adapter discovery, ECU
// timeouts). Rather than fight that with a hand-rolled non-blocking AT-command
// scheduler that still bottoms out on a blocking connect(), this class runs
// entirely on its own FreeRTOS task pinned to config::kObdTaskCore, away from
// the Arduino loop task. That satisfies the project's "never block the
// display loop on Bluetooth I/O" requirement directly: the render loop keeps
// running at a stable cadence no matter how long a reconnect attempt takes.
// All state shared with the render loop crosses through a mutex (snapshot_,
// dtcResult_) or std::atomic (connectionState_ and the request/ready flags).
class ObdClient {
public:
    ObdClient() = default;
    ~ObdClient() = default;

    ObdClient(const ObdClient&) = delete;
    ObdClient& operator=(const ObdClient&) = delete;

    // Starts the background task. Call once from setup(), after ConfigStore
    // has loaded, with the persisted adapter identity (ignored in simulation
    // builds; also ignored on real hardware if secrets/local_config.h is
    // present, since that always takes priority).
    bool begin(const char* adapterName, const char* adapterPin);

    // Thread-safe: updates the adapter identity used for the next (and all
    // subsequent) connection attempts and forces an immediate reconnect
    // instead of waiting for the current backoff/poll cycle. No-op in
    // simulation builds and when secrets/local_config.h is present.
    void updateAdapterCredentials(const char* adapterName, const char* adapterPin);

    // Thread-safe copy of the latest telemetry snapshot.
    void getSnapshot(TelemetrySnapshot& out) const;

    // Transport state for the status badge. Only ever ObdConnecting,
    // Reconnecting, or Live; main.cpp derives Stale/Degraded/Boot itself.
    ConnectionState getConnectionState() const { return connectionState_.load(); }

    // Queues an on-demand Mode 03 + Mode 07 DTC read (Page 5). Safe to call
    // repeatedly; the background task clears the request once it runs it.
    void requestDtcRead() { dtcReadRequested_.store(true); }

    // Queues a Mode 04 "clear codes" command (Page 5).
    void requestClearCodes() { clearCodesRequested_.store(true); }

    // Thread-safe copy of the most recent completed DTC read result.
    void getDtcList(DtcList& out) const;

    // True once a DTC read has completed at least once this session.
    bool hasDtcResult() const { return dtcResultReady_.load(); }

private:
    static void taskEntry(void* param);
    void setSnapshotConnected(bool connected);

#if OBD_SIMULATION_ENABLED
    void simulationLoop();
    void runSimulationBlip();

    ObdSimulator simulator_;
#else
    void taskLoop();

    bool runInitSequence();
    bool sendCommand(const char* command, char* responseOut, size_t responseCapacity, uint32_t timeoutMs);
    void pollPid(ObdPid id, uint32_t nowMs);
    void performDtcRead();
    void performClearCodes();

    BluetoothSerial btSerial_;

    // Adapter identity currently in use by taskLoop(); adapterName_/adapterPin_
    // are only ever touched from within taskLoop() itself. updateAdapterCredentials()
    // writes the pending fields under mutex_ and sets credentialsChanged_;
    // taskLoop() picks them up at the top of its next loop iteration.
    char adapterName_[24] = {0};
    char adapterPin_[9] = {0};
    char pendingAdapterName_[24] = {0}; // Guarded by mutex_
    char pendingAdapterPin_[9] = {0};   // Guarded by mutex_
    std::atomic<bool> credentialsChanged_{false};
#endif

    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t taskHandle_ = nullptr;

    TelemetrySnapshot snapshot_; // Guarded by mutex_
    DtcList dtcResult_;          // Guarded by mutex_

    std::atomic<ConnectionState> connectionState_{ConnectionState::ObdConnecting};
    std::atomic<bool> dtcReadRequested_{false};
    std::atomic<bool> clearCodesRequested_{false};
    std::atomic<bool> dtcResultReady_{false};

#if !OBD_SIMULATION_ENABLED
    // Task-local; only ever touched from within taskLoop(), so plain fields.
    uint8_t secondaryPollIndex_ = 0;
    uint8_t consecutiveFailures_ = 0;
    uint32_t reconnectBackoffMs_ = 0;
#endif
};
