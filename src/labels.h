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
    "OBD ADAPTER",
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

// ---- Config: OBD Adapter ----
constexpr const char* kLabelObdAdapterName = "ADAPTER NAME";
constexpr const char* kLabelObdAdapterPin = "ADAPTER PIN";
constexpr const char* kHintObdReconnectOnSave = "SAVE RECONNECTS IMMEDIATELY";

// ---- Config: Buttons & Status ----
constexpr const char* kStepperMinus = "-";
constexpr const char* kStepperPlus = "+";
constexpr const char* kButtonTapToConfirm = "TAP TO CONFIRM";
constexpr const char* kButtonDeleteAllLogs = "DELETE ALL LOGS";
constexpr const char* kButtonSaved = "SAVED!";
constexpr const char* kButtonSaveToSd = "SAVE TO SD";

// ---- Page 6: Diagnostics ----
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

// ---- Theme ----
constexpr const char* kThemeNameModernFlat = "Modern Flat";
constexpr const char* kThemeNameNeon = "Neon";
constexpr const char* kThemeNameS197 = "S197";

} // namespace labels
