#pragma once

#include <stdint.h>

#include "obd/telemetry.h"
#include "system/dtc_decoder.h"

// Generates a scripted, deterministic drive cycle so the whole cluster UI can be
// exercised on the bench with no ELM327 adapter or vehicle present. Only compiled
// into OBD_SIMULATION_ENABLED builds; see ObdClient::simulationLoop().
//
// RPM, speed, and throttle come from a fixed segment table (idle -> three gear
// pulls -> cruise -> decel fuel cut -> stop-and-go -> idle); every other PID is
// derived from those three so no two readings contradict each other. Jitter uses
// a fixed-seed LCG, never hardware entropy, so a run is reproducible.
class ObdSimulator {
public:
    void reset();

    // deltaMs is real elapsed time; it is scaled by config::kSimTimeScale before
    // advancing the cycle. Callers freeze the cycle simply by not calling this.
    void update(uint32_t deltaMs, uint32_t nowMs, TelemetrySnapshot& out);

    void fillDtcList(DtcList& out) const;
    void clearDtcs();

private:
    bool isMilActive() const { return sessionElapsedMs_ >= dtcAppearAtMs_; }
    float jitter(float amplitude);

    float cycleElapsedMs_ = 0.0F;
    float sessionElapsedMs_ = 0.0F;
    float dtcAppearAtMs_ = 0.0F;
    uint32_t rngState_ = 0x13572468U;
};
