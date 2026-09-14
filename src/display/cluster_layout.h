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
// Left to right: prev arrow, page title (display-only), OBDII status badge
// (display-only), MIL/CEL zone, mode toggle (dashboard/config groups), next
// arrow.
constexpr int32_t kNavPrevX0 = 0, kNavPrevX1 = 60;
constexpr int32_t kNavNextX0 = 420, kNavNextX1 = 480;

// The mode toggle icon sits this far from its adjacent nav arrow.
constexpr int32_t kHeaderIconGap = 8;

constexpr int32_t kBadgeW = 90, kBadgeH = 22, kBadgeY = 9;
constexpr int32_t kBadgeX = (kScreenWidth - kBadgeW) / 2;

// Page title: centered between the prev arrow and the OBDII badge (display only).
constexpr int32_t kNavTitleCenterX = (kNavPrevX1 + kBadgeX) / 2;

constexpr int32_t kMilZoneX0 = 310, kMilZoneX1 = 365;
constexpr int32_t kMilCenterX = (kMilZoneX0 + kMilZoneX1) / 2;
constexpr int32_t kMilCenterY = kHeaderHeight / 2;

// Mode toggle touch zone stays between the MIL zone and the next arrow, but
// the icon itself is drawn kHeaderIconGap from the next arrow (mirroring the
// SD light's offset from the prev arrow) rather than at the zone's midpoint.
constexpr int32_t kModeToggleX0 = 365, kModeToggleX1 = 420;
constexpr int32_t kModeToggleCenterX = kNavNextX0 - kHeaderIconGap;
constexpr int32_t kModeToggleCenterY = kHeaderHeight / 2;

// Config pages (UI, Logs, ...): each page owns rows 0..3 of its own 40px-tall
// body independently (same convention as every other page), plus a Save
// button footer shared by every config page, fixed to the bottom of the
// screen regardless of which config page is active or how many rows it uses.
constexpr int32_t kConfigRowHeight = 40;
constexpr int32_t kConfigRow0Y = kHeaderHeight;
constexpr int32_t kConfigRow1Y = kConfigRow0Y + kConfigRowHeight;
constexpr int32_t kConfigRow2Y = kConfigRow1Y + kConfigRowHeight;
constexpr int32_t kConfigRow3Y = kConfigRow2Y + kConfigRowHeight;
// Rows 4-5: the row grid stops here rather than continuing indefinitely
// because this is exactly where it hits the shared Save footer below - only
// GAUGES (6 rows) currently reaches this far.
constexpr int32_t kConfigRow4Y = kConfigRow3Y + kConfigRowHeight;
constexpr int32_t kConfigRow5Y = kConfigRow4Y + kConfigRowHeight;

constexpr int32_t kConfigFooterY = kScreenHeight - kConfigRowHeight; // Shared save button

constexpr int32_t kConfigMinusX = 240, kConfigMinusW = 50;
constexpr int32_t kConfigValueX = 300, kConfigValueW = 90;
constexpr int32_t kConfigPlusX = 400, kConfigPlusW = 50;
constexpr int32_t kConfigButtonInsetY = 4;
constexpr int32_t kConfigButtonH = kConfigRowHeight - 2 * kConfigButtonInsetY;

constexpr int32_t kConfigCycleX = 240, kConfigCycleW = 210;   // Logs row 0: tap-to-cycle log interval
constexpr int32_t kConfigSaveX = 20, kConfigSaveW = 440;      // Footer: full-width save button
constexpr int32_t kConfigDeleteX = 300, kConfigDeleteW = 160; // Logs: delete-all-logs button

// Logs page only: Log Units + its warning line form a taller section than
// the standard 40px row, so the Log Summary/Delete row that follows it is
// pushed down and no longer lines up with the shared row grid.
constexpr int32_t kLogsSummaryRowY = kConfigRow1Y + 58; // 138

// Page 4 (Performance & Telemetry): 0-60 MPH timer tap-to-reset box.
constexpr int32_t kPerfTimerX0 = 140, kPerfTimerY0 = 125, kPerfTimerX1 = 340, kPerfTimerY1 = 195;

// Page 6 (Diagnostics).
constexpr int32_t kDtcListY = 85;
// Sized for the theme's tier-2 value font (see applyValueFont()), whose
// tallest case (Modern Flat's FreeSans12pt7b) is ~29px, vs. the 20px row
// height the plain default font used before the DTC list switched fonts.
constexpr int32_t kDtcListLineHeight = 32;
constexpr uint8_t kDtcListVisibleLines = 5;
constexpr int32_t kDtcButtonY = 270;
constexpr int32_t kDtcButtonH = 40;
constexpr int32_t kDtcReadButtonX = 40, kDtcReadButtonW = 180;
constexpr int32_t kDtcClearButtonX = 260, kDtcClearButtonW = 180;

} // namespace layout
