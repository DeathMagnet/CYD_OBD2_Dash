#pragma once

#include <stdint.h>
#include "app_config.h"

// Shared pixel geometry for the cluster UI, used by both cluster_pages.cpp
// (drawing) and touch_handler.cpp (hit-testing), so the two can never drift
// apart the way separately-maintained constants could.
namespace layout {

constexpr int32_t kHeaderHeight = config::kHeaderHeight; // 40
constexpr int32_t kScreenWidth = config::kScreenWidth;   // 480
constexpr int32_t kScreenHeight = config::kScreenHeight; // 320

// Header touch zones (docs/cyd-obd2-ui-cluster-guide.md: Touch Navigation).
constexpr int32_t kNavPrevX0 = 0, kNavPrevX1 = 60;
constexpr int32_t kNavNextX0 = 420, kNavNextX1 = 480;
constexpr int32_t kNavTitleX0 = 61, kNavTitleX1 = 350;
constexpr int32_t kMilZoneX0 = 351, kMilZoneX1 = 419;
constexpr int32_t kMilCenterX = (kMilZoneX0 + kMilZoneX1) / 2;
constexpr int32_t kMilCenterY = kHeaderHeight / 2;

// Page 5 (Config Menu): 7 rows of 40px exactly filling the 280px body.
constexpr int32_t kConfigRowHeight = 40;
constexpr int32_t kConfigRow0Y = kHeaderHeight;                   // Theme (display only)
constexpr int32_t kConfigRow1Y = kConfigRow0Y + kConfigRowHeight; // Shift light RPM
constexpr int32_t kConfigRow2Y = kConfigRow1Y + kConfigRowHeight; // Redline RPM
constexpr int32_t kConfigRow3Y = kConfigRow2Y + kConfigRowHeight; // Log interval
constexpr int32_t kConfigRow4Y = kConfigRow3Y + kConfigRowHeight; // Boost baro baseline
constexpr int32_t kConfigRow5Y = kConfigRow4Y + kConfigRowHeight; // Save button
constexpr int32_t kConfigRow6Y = kConfigRow5Y + kConfigRowHeight; // Log summary + delete

constexpr int32_t kConfigMinusX = 240, kConfigMinusW = 50;
constexpr int32_t kConfigValueX = 300, kConfigValueW = 90;
constexpr int32_t kConfigPlusX = 400, kConfigPlusW = 50;
constexpr int32_t kConfigButtonInsetY = 4;
constexpr int32_t kConfigButtonH = kConfigRowHeight - 2 * kConfigButtonInsetY;

constexpr int32_t kConfigCycleX = 240, kConfigCycleW = 210;   // Row 3: tap-to-cycle log interval
constexpr int32_t kConfigSaveX = 20, kConfigSaveW = 440;      // Row 5: full-width save button
constexpr int32_t kConfigDeleteX = 300, kConfigDeleteW = 160; // Row 6: delete-all-logs button

// Page 4 (Performance & Telemetry): 0-60 MPH timer tap-to-reset box.
constexpr int32_t kPerfTimerX0 = 140, kPerfTimerY0 = 125, kPerfTimerX1 = 340, kPerfTimerY1 = 180;

// Page 6 (Diagnostics).
constexpr int32_t kDtcListY = 70;
constexpr int32_t kDtcListLineHeight = 20;
constexpr uint8_t kDtcListVisibleLines = 8;
constexpr int32_t kDtcButtonY = 270;
constexpr int32_t kDtcButtonH = 40;
constexpr int32_t kDtcReadButtonX = 40, kDtcReadButtonW = 180;
constexpr int32_t kDtcClearButtonX = 260, kDtcClearButtonW = 180;

} // namespace layout
