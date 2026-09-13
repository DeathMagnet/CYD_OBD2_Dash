#pragma once

#include <stdint.h>
#include <stddef.h>

namespace config {

// Display Configuration (ST7796S 4.0-inch CYD)
constexpr uint16_t kScreenWidth = 480;
constexpr uint16_t kScreenHeight = 320;
constexpr uint8_t kDisplayRotation = 1; // Landscape (480x320)
constexpr uint8_t kTftBacklightPin = 27;

// Boot Screen Timing
constexpr uint32_t kBootScreenDurationMs = 2000;

// SD Card Pins (VSPI)
constexpr uint8_t kSdSpiCsPin = 5;
constexpr uint8_t kSdSpiMosiPin = 23;
constexpr uint8_t kSdSpiMisoPin = 19;
constexpr uint8_t kSdSpiClkPin = 18;
constexpr uint32_t kSdSpiFrequency = 20000000; // 20 MHz

// Touch Pins (Hosyond 4.0" CYD XPT2046 over TFT SPI)
constexpr uint8_t kTouchCsPin   = 33;

// Touch Calibration File & Array Size
constexpr const char* kTouchCalFilePath = "/touch_cal.dat";
constexpr size_t kTouchCalDataSize = 5;

// Touch Polling Interval & Pressure Threshold
constexpr uint32_t kTouchPollIntervalMs = 20;
constexpr uint16_t kTouchPressureThreshold = 200;

// ---- OBD-II / ELM327 Bluetooth Configuration ----
// Adapter identity defaults; override per-device via src/secrets/local_config.h
// (see src/secrets/local_config.example.h) rather than editing these.
constexpr const char* kObdDefaultAdapterName = "OBDII";
constexpr const char* kObdDefaultAdapterPin = "1234";

constexpr uint32_t kObdResetCommandTimeoutMs = 3000;   // ATZ reset needs extra settle time
constexpr uint32_t kObdCommandTimeoutMs = 1000;        // Normal AT/PID command timeout
constexpr uint32_t kObdReconnectBackoffMs = 3000;
constexpr uint32_t kObdMaxReconnectBackoffMs = 30000;
constexpr uint8_t kObdConsecutiveFailuresForDisconnect = 6;
constexpr uint32_t kObdTaskStackWords = 8192;          // FreeRTOS task stack, in words (uint32_t units)
constexpr uint8_t kObdTaskPriority = 1;
constexpr int8_t kObdTaskCore = 0;                     // Keep Bluetooth I/O off the Arduino loop core (1)
constexpr uint32_t kObdSecondaryPidIntervalMs = 40;    // Pace between queued command sends
constexpr uint32_t kTelemetryStaleThresholdMs = 3000;  // No fresh update within this window -> STALE badge

// ---- OBD-II Simulation (only used by OBD_SIMULATION_ENABLED builds) ----
// Multiplies the scripted drive cycle's clock so a full lap can be swept faster
// during manual UI checks. Override per-build with -D SIM_TIME_SCALE=2.0F.
#ifndef SIM_TIME_SCALE
#define SIM_TIME_SCALE 1.0F
#endif
constexpr float kSimTimeScale = SIM_TIME_SCALE;

constexpr uint32_t kSimConnectDelayMs = 2500;          // Fake adapter handshake before LIVE
constexpr uint32_t kSimDtcAppearAfterMs = 20000;       // Cycle time before MIL + fake codes latch
constexpr uint32_t kSimWarmupMs = 120000;              // Cold-to-operating coolant ramp
constexpr float kSimColdCoolantF = 72.0F;
constexpr float kSimHotCoolantF = 196.0F;

// Periodic dropout that walks the status badge through LIVE -> STALE ->
// RECONNECTING -> CONNECTING -> LIVE. Scheduled off real millis(), never
// kSimTimeScale, so STALE detection stays true to production timing.
constexpr bool kSimReconnectBlipEnabled = true;
constexpr uint32_t kSimReconnectBlipIntervalMs = 45000;
constexpr uint32_t kSimBlipStaleHoldMs = 4000;         // Must exceed kTelemetryStaleThresholdMs
constexpr uint32_t kSimBlipReconnectingMs = 4000;
constexpr uint32_t kSimBlipConnectingMs = 1500;

// Drives coolant past kHighCoolantWarningF and voltage below kLowVoltageWarningV
// so the warning paths on Pages 1/3 are reachable without a fault injection.
constexpr bool kSimWarningSweepEnabled = true;
constexpr uint32_t kSimWarningSweepIntervalMs = 180000;
constexpr uint32_t kSimWarningSweepDurationMs = 12000;

// ---- Warning / Tunable Dashboard Thresholds (persisted defaults) ----
constexpr uint16_t kMinShiftLightRpm = 3000;
constexpr uint16_t kMaxShiftLightRpm = 6800;
constexpr uint16_t kShiftLightStepRpm = 100;
constexpr uint16_t kDefaultShiftLightRpm = 5800;

constexpr uint16_t kMinRedlineRpm = 5000;
constexpr uint16_t kMaxRedlineRpm = 7000;
constexpr uint16_t kRedlineStepRpm = 100;
constexpr uint16_t kDefaultRedlineRpm = 6200;

constexpr uint16_t kMinMaxRpm = kMaxRedlineRpm; // Never below the highest possible redline.
constexpr uint16_t kMaxMaxRpm = 9000;
constexpr uint16_t kMaxRpmStepRpm = 100;
constexpr uint16_t kDefaultMaxRpm = 7000; // Matches the previous hardcoded gauge scale.

constexpr uint16_t kMinMaxSpeedMph = 120;
constexpr uint16_t kMaxMaxSpeedMph = 260;
constexpr uint16_t kMaxSpeedStepMph = 10;
constexpr uint16_t kDefaultMaxSpeedMph = 200; // Matches the previous hardcoded gauge scale.

constexpr float kMinBaroBaselinePsi = 12.0F;
constexpr float kMaxBaroBaselinePsi = 15.5F;
constexpr float kBaroBaselineStepPsi = 0.1F;
constexpr float kDefaultBaroBaselinePsi = 14.7F;

constexpr float kHighCoolantWarningF = 220.0F;
constexpr float kLowVoltageWarningV = 11.5F;

// ---- SD CSV Telemetry Logging ----
constexpr const char* kLogFilePrefix = "/mustang_log_";
constexpr const char* kLogFileSuffix = ".csv";
constexpr uint8_t kLogFileIndexDigits = 3;
constexpr const char* kSessionIndexNamespace = "obd_dash";
constexpr const char* kSessionIndexKey = "session_idx";

constexpr uint32_t kLogRowIntervalOptionsMs[] = {50, 100, 250, 500, 1000};
constexpr size_t kLogRowIntervalOptionCount = 5;
constexpr uint32_t kDefaultLogRowIntervalMs = 100;
constexpr uint32_t kSdFlushIntervalMs = 500;

// Auto-pruning: once free space drops below this floor, delete the oldest
// session log(s) to make room for the active session before every write.
constexpr uint64_t kSdMinFreeBytes = 5ULL * 1024 * 1024; // 5 MB headroom
constexpr uint8_t kSdPruneBatchCount = 1;

// ---- Page / Theme ----
constexpr uint8_t kPageCount = 6;
constexpr uint8_t kHeaderHeight = 40;
constexpr uint32_t kUiRefreshIntervalMs = 100; // Throttled dynamic-region redraw cadence

// ---- Persisted Config File ----
constexpr const char* kConfigFilePath = "/config.txt";

} // namespace config
