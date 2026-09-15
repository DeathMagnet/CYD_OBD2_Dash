#pragma once

namespace labels {

// ---- Navigation / Header ----
constexpr const char* kNavPrev = "<";
constexpr const char* kNavNext = ">";

constexpr const char* kPageTitles[] = {
    "PRIMARY CLUSTER",
    "ENGINE LOAD",
    "CAR-SPECIFIC",
    "PERFORMANCE",
    "DIAGNOSTICS",
    "UI",
    "GAUGES",
    "USER VARS",
    "LOGS",
};

// ---- Page 1: Primary Cluster ----
constexpr const char* kUnitRpm = "RPM";
constexpr const char* kUnitMph = "MPH";
constexpr const char* kUnitKph = "KM/H";
constexpr const char* kUnitCelsius = "C";
constexpr const char* kUnitFahrenheit = "F";
constexpr const char* kUnitKpa = "KPA";
constexpr const char* kLabelCoolant = "COOLANT";
constexpr const char* kLabelIat = "IAT";
constexpr const char* kLabelThrottle = "THROTTLE";

// ---- Page 2: Engine Load & Airflow ----
constexpr const char* kLabelEngineLoad = "ENGINE LOAD";
constexpr const char* kLabelMaf = "MAF";
constexpr const char* kLabelTimingAdvance = "TIMING ADVANCE";
constexpr const char* kLabelStftBank1 = "STFT BANK 1";
constexpr const char* kLabelLtftBank1 = "LTFT BANK 1";

// ---- Page 3: Car-Specific Sensors ----
constexpr const char* kLabelBatteryVoltage = "BATTERY VOLTAGE";
constexpr const char* kLabelFuelRailPressure = "FUEL RAIL PRESSURE";
constexpr const char* kLabelBoost = "BOOST";
constexpr const char* kLabelVacuum = "VACUUM";
constexpr const char* kLabelVacBoost = "VAC/BOOST";
constexpr const char* kUnitPsi = "PSI";
constexpr const char* kUnitInHg = "inHg";
constexpr const char* kLabelO2B1S1 = "O2 B1S1";
constexpr const char* kLabelO2B2S1 = "O2 B2S1";

// ---- Page 4: Performance & Telemetry ----
constexpr const char* kLabelIntakeAirflow60s = "INTAKE AIRFLOW (LAST 60s)";
constexpr const char* kLabelEstHorsepower = "EST. HORSEPOWER";
constexpr const char* kLabelEstTorque = "EST. TORQUE";
constexpr const char* kLabelZeroToSixty = "0-60 MPH (TAP TO RESET)";
constexpr const char* kLabelZeroToHundredKph = "0-100 KM/H (TAP TO RESET)";

// ---- Config: UI ----
constexpr const char* kLabelActiveTheme = "ACTIVE THEME";
constexpr const char* kLabelUnits = "UNITS";
constexpr const char* kLabelUnitsStandard = "STANDARD (MPH/F/PSI)";
constexpr const char* kLabelUnitsMetric = "METRIC (KM/H/C/KPA)";
constexpr const char* kLabelGaugeTicks = "GAUGE TICKS";
constexpr const char* kLabelTickModeOff = "TICS OFF";
constexpr const char* kLabelTickModeInsideOnly = "INSIDE TICS ONLY";
constexpr const char* kLabelTickModeOutsideOnly = "OUTSIDE TICS ONLY";
constexpr const char* kLabelTickModeInsideAndOutside = "INSIDE AND OUTSIDE TICS";
constexpr const char* kLabelFlipScreen = "FLIP SCREEN 180";
constexpr const char* kLabelFlipScreenNormal = "NORMAL";
constexpr const char* kLabelFlipScreenFlipped = "FLIPPED 180";
constexpr const char* kWarningFlipScreenRestarts = "CHANGING THIS RESTARTS DEVICE ON SAVE";
constexpr const char* kLabelTouchCalibration = "TOUCH CALIBRATION";
constexpr const char* kButtonRecalibrateTouch = "RECALIBRATE";

// ---- Config: User Vars ----
constexpr const char* kLabelBoostBaroBaseline = "BOOST BARO BASELINE";
constexpr const char* kLabelCoolantWarningTemp = "COOLANT TEMP WARN";
constexpr const char* kLabelLowVoltageWarning = "LOW VOLTAGE WARNING";
constexpr const char* kLabelBoostGaugeMax = "BOOST GAUGE MAX";
constexpr const char* kLabelVacuumGaugeMax = "VACUUM GAUGE MAX";
constexpr const char* kLabelZeroSixtyTarget = "0-60 TARGET SPEED";
constexpr const char* kLabelHpEstimationFactor = "HP ESTIMATE FACTOR";
constexpr const char* kLabelFuelTrimRange = "FUEL TRIM RANGE";

// ---- Config: Gauges ----
constexpr const char* kLabelShiftLightRpm = "SHIFT LIGHT RPM";
constexpr const char* kLabelRedlineRpm = "REDLINE RPM";
constexpr const char* kLabelMaxRpm = "MAX RPM";
constexpr const char* kLabelMaxSpeedMph = "MAX SPEED MPH";
constexpr const char* kLabelMaxSpeedKph = "MAX SPEED KM/H";

// ---- Config: Logs ----
constexpr const char* kLabelLogInterval = "LOG INTERVAL";
constexpr const char* kLabelLogUnits = "LOG UNITS";
constexpr const char* kWarningLogUnitsDeletesLogs = "CHANGING THIS DELETES ALL LOGS ON SAVE";

// ---- Config: Buttons & Status ----
constexpr const char* kStepperMinus = "-";
constexpr const char* kStepperPlus = "+";
constexpr const char* kButtonTapToConfirm = "TAP TO CONFIRM";
constexpr const char* kButtonDeleteAllLogs = "DELETE ALL LOGS";
constexpr const char* kButtonSaved = "SAVED!";
constexpr const char* kButtonSaveToSd = "SAVE TO SD";

// ---- Page 5: Diagnostics ----
constexpr const char* kButtonRefreshCodes = "REFRESH CODES";
constexpr const char* kStatusMilActive = "MIL: ACTIVE (ON)";
constexpr const char* kStatusMilInactive = "MIL: INACTIVE (OFF)";
constexpr const char* kStatusReadingCodes = "Reading codes...";
constexpr const char* kStatusNoCodes = "No stored or pending codes.";
constexpr const char* kButtonClearCodes = "CLEAR CODES";

// ---- Connection Status Badge ----
constexpr const char* kStatusBoot = "BOOT";
constexpr const char* kStatusDisplayReady = "DISPLAY READY";
constexpr const char* kStatusSdInit = "SD INIT";
constexpr const char* kStatusObdConnecting = "CONNECTING";
constexpr const char* kStatusLive = "LIVE";
constexpr const char* kStatusStale = "STALE";
constexpr const char* kStatusReconnecting = "RECONNECTING";
constexpr const char* kStatusDegraded = "NO OBD";
constexpr const char* kStatusUnknown = "UNKNOWN";

// ---- Bluetooth Pairing Screen (boot-time only, see obd/obd_pairing.h) ----
constexpr const char* kPairingTitle = "PAIR OBD-II ADAPTER";
constexpr const char* kPairingDevicePrefix = "DEVICE: ";
constexpr const char* kPairingDeviceEmpty = "DEVICE: (tap to scan)";
constexpr const char* kPairingPasswordPrefix = "PASSWORD: ";
constexpr const char* kButtonConnect = "CONNECT";
constexpr const char* kStatusConnecting = "CONNECTING...";
constexpr const char* kStatusPairingScanning = "Scanning for adapters...";
constexpr const char* kStatusPairingScanEmpty = "No devices found - tap DEVICE to retry.";
constexpr const char* kStatusPairingTapDeviceFirst = "Scan first - tap DEVICE.";

// ---- Theme ----
constexpr const char* kThemeNameModernFlat = "Modern Flat";
constexpr const char* kThemeNameNeon = "Neon";
constexpr const char* kThemeNameS197Digital = "S197 - Digital";
constexpr const char* kThemeNameS197Analog = "S197 - Analog";

} // namespace labels
