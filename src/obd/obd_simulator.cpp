#include "obd/obd_simulator.h"

#include <math.h>
#include <string.h>

#include "app_config.h"

namespace {

// One scripted leg of the drive cycle. Values are the targets reached at the end
// of the leg; the leg interpolates from the previous leg's targets. The last
// entry closes on the first entry's values so the loop wraps without a jump.
struct DriveSegment {
    uint32_t durationMs;
    float endRpm;
    float endSpeedMph;
    float endThrottlePct;
    bool isShift; // Linear rather than eased, for a crisp needle drop.
};

constexpr DriveSegment kDriveCycle[] = {
    {8000,  750.0F,  0.0F,  0.0F,  false}, // Idle
    {4000,  5200.0F, 35.0F, 85.0F, false}, // Launch, 1st gear
    {400,   3000.0F, 36.0F, 8.0F,  true},  // 1-2 shift
    {4000,  5900.0F, 60.0F, 90.0F, false}, // 2nd gear pull, crosses the shift light
    {400,   3400.0F, 61.0F, 8.0F,  true},  // 2-3 shift
    {5000,  6250.0F, 95.0F, 95.0F, false}, // 3rd gear pull, touches the redline arc
    {400,   2900.0F, 95.0F, 8.0F,  true},  // 3-4 shift
    {25000, 2200.0F, 70.0F, 18.0F, false}, // Cruise
    {20000, 2600.0F, 80.0F, 22.0F, false}, // Highway
    {6000,  1200.0F, 25.0F, 0.0F,  false}, // Decel fuel cut
    {12000, 1500.0F, 8.0F,  25.0F, false}, // Stop-and-go
    {10000, 750.0F,  0.0F,  0.0F,  false}, // Idle return
};
constexpr size_t kSegmentCount = sizeof(kDriveCycle) / sizeof(kDriveCycle[0]);

// Caps the advance applied after a long stall (task starvation, reconnect blip)
// so the cycle creeps forward instead of teleporting.
constexpr float kMaxTickMs = 250.0F;

float clampF(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

float lerpF(float from, float to, float t) {
    return from + (to - from) * t;
}

float smoothStep(float t) {
    return t * t * (3.0F - 2.0F * t);
}

uint32_t cycleTotalMs() {
    static uint32_t total = 0;
    if (total == 0) {
        for (size_t i = 0; i < kSegmentCount; ++i) {
            total += kDriveCycle[i].durationMs;
        }
    }
    return total;
}

} // namespace

void ObdSimulator::reset() {
    cycleElapsedMs_ = 0.0F;
    sessionElapsedMs_ = 0.0F;
    dtcAppearAtMs_ = static_cast<float>(config::kSimDtcAppearAfterMs);
    rngState_ = 0x13572468U;
}

float ObdSimulator::jitter(float amplitude) {
    rngState_ = rngState_ * 1664525U + 1013904223U;
    float unit = static_cast<float>((rngState_ >> 16) & 0xFFFFU) / 32767.5F - 1.0F;
    return unit * amplitude;
}

void ObdSimulator::update(uint32_t deltaMs, uint32_t nowMs, TelemetrySnapshot& out) {
    float scaledDeltaMs = fminf(static_cast<float>(deltaMs), kMaxTickMs) * config::kSimTimeScale;
    cycleElapsedMs_ += scaledDeltaMs;
    sessionElapsedMs_ += scaledDeltaMs;

    float totalMs = static_cast<float>(cycleTotalMs());
    while (cycleElapsedMs_ >= totalMs) {
        cycleElapsedMs_ -= totalMs;
    }

    float cursorMs = cycleElapsedMs_;
    size_t index = 0;
    while (index < kSegmentCount - 1 && cursorMs >= static_cast<float>(kDriveCycle[index].durationMs)) {
        cursorMs -= static_cast<float>(kDriveCycle[index].durationMs);
        ++index;
    }

    const DriveSegment& segment = kDriveCycle[index];
    const DriveSegment& previous = kDriveCycle[(index + kSegmentCount - 1) % kSegmentCount];
    float progress = clampF(cursorMs / static_cast<float>(segment.durationMs), 0.0F, 1.0F);
    float blend = segment.isShift ? progress : smoothStep(progress);

    float rpm = lerpF(previous.endRpm, segment.endRpm, blend) + jitter(12.0F);
    float speedMph = clampF(lerpF(previous.endSpeedMph, segment.endSpeedMph, blend), 0.0F, 200.0F);
    float throttlePct = clampF(lerpF(previous.endThrottlePct, segment.endThrottlePct, blend), 0.0F, 100.0F);

    bool isIdling = (speedMph < 1.0F) && (throttlePct < 1.0F);
    bool isFuelCut = (throttlePct < 1.0F) && (speedMph > 2.0F); // Decel fuel shutoff
    bool isWideOpen = throttlePct > 80.0F;

    float engineLoadPct = isFuelCut
        ? 3.0F + jitter(0.5F)
        : clampF(9.0F + throttlePct * 0.82F + (rpm / 6500.0F) * 7.0F, 0.0F, 100.0F);

    float mapKpa = isFuelCut ? 20.0F + jitter(1.0F) : 30.0F + throttlePct * 0.68F + jitter(0.8F);
    float mafGps = clampF((rpm / 1000.0F) * (mapKpa / 100.0F) * 9.0F, 0.5F, 120.0F);

    float timingAdvanceDeg = isIdling
        ? 14.0F + jitter(0.8F)
        : clampF(34.0F - throttlePct * 0.22F + (rpm / 6500.0F) * 2.0F, 5.0F, 45.0F);

    float o2B1S1V;
    if (isFuelCut) {
        o2B1S1V = 0.08F;
    } else if (isWideOpen) {
        o2B1S1V = 0.86F + jitter(0.02F);
    } else {
        // Closed-loop switching at roughly 1.5 Hz.
        o2B1S1V = 0.45F + 0.33F * sinf(cycleElapsedMs_ * 0.0094F);
    }
    float o2B2S1V = clampF(o2B1S1V + jitter(0.04F), 0.0F, 1.27F);
    o2B1S1V = clampF(o2B1S1V, 0.0F, 1.27F);

    float ltftPct = 1.5F + sinf(sessionElapsedMs_ * 0.00008F) * 2.0F;
    float stftPct;
    if (isFuelCut) {
        stftPct = -22.0F;
    } else if (isWideOpen) {
        stftPct = jitter(0.5F);
    } else {
        stftPct = -ltftPct * 0.4F + jitter(3.0F);
    }

    bool isWarningSweep = false;
    if (config::kSimWarningSweepEnabled && sessionElapsedMs_ > static_cast<float>(config::kSimWarningSweepIntervalMs)) {
        float phaseMs = fmodf(sessionElapsedMs_, static_cast<float>(config::kSimWarningSweepIntervalMs));
        isWarningSweep = phaseMs < static_cast<float>(config::kSimWarningSweepDurationMs);
    }

    float warmFraction = clampF(sessionElapsedMs_ / static_cast<float>(config::kSimWarmupMs), 0.0F, 1.0F);
    float coolantF = lerpF(config::kSimColdCoolantF, config::kSimHotCoolantF, smoothStep(warmFraction)) +
                     sinf(sessionElapsedMs_ * 0.00035F) * 3.0F;
    if (isWarningSweep) {
        coolantF = config::kDefaultCoolantWarningF + 12.0F + jitter(1.5F);
    }

    float voltageV = isIdling ? 13.6F : 14.1F - (engineLoadPct / 100.0F) * 0.45F;
    if (isWarningSweep) {
        voltageV = config::kDefaultLowVoltageWarningV - 0.4F + jitter(0.05F);
    }

    float iatF = 84.0F + 18.0F * (1.0F - clampF(speedMph / 45.0F, 0.0F, 1.0F)) + jitter(0.6F);
    float fuelPct = clampF(87.0F - (sessionElapsedMs_ / 600000.0F) * 3.0F, 12.0F, 100.0F);
    float fuelPressureKpa = 380.0F + throttlePct * 0.4F + jitter(2.0F);
    float baroKpa = 101.0F;

    bool milActive = isMilActive();

    out.rpm = {clampF(rpm, 0.0F, 8000.0F), true, nowMs};
    out.speedMph = {speedMph, true, nowMs};
    out.throttlePct = {throttlePct, true, nowMs};
    out.engineLoadPct = {engineLoadPct, true, nowMs};
    out.mapKpa = {mapKpa, true, nowMs};
    out.mafGps = {mafGps, true, nowMs};
    out.timingAdvanceDeg = {timingAdvanceDeg, true, nowMs};
    out.o2B1S1V = {o2B1S1V, true, nowMs};
    out.o2B2S1V = {o2B2S1V, true, nowMs};
    out.stftPct = {stftPct, true, nowMs};
    out.ltftPct = {ltftPct, true, nowMs};
    out.coolantF = {coolantF, true, nowMs};
    out.iatF = {iatF, true, nowMs};
    out.voltageV = {voltageV, true, nowMs};
    out.fuelPct = {fuelPct, true, nowMs};
    out.fuelPressureKpa = {fuelPressureKpa, true, nowMs};
    out.baroKpa = {baroKpa, true, nowMs};
    out.milOn = {milActive ? 1.0F : 0.0F, true, nowMs};
    out.dtcCount = {milActive ? 3.0F : 0.0F, true, nowMs};

    out.connected = true;
    out.capturedAtMs = nowMs;
}

void ObdSimulator::fillDtcList(DtcList& out) const {
    out = DtcList();
    if (!isMilActive()) {
        return;
    }

    static const char* const kSimulatedCodes[] = {"P0133", "P0171", "P0455"};
    for (const char* code : kSimulatedCodes) {
        if (out.count >= kMaxDtcCount) {
            break;
        }
        strncpy(out.codes[out.count], code, kDtcCodeLength - 1);
        out.codes[out.count][kDtcCodeLength - 1] = '\0';
        out.count++;
    }
}

void ObdSimulator::clearDtcs() {
    dtcAppearAtMs_ = sessionElapsedMs_ + static_cast<float>(config::kSimDtcAppearAfterMs);
}
