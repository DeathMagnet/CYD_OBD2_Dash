#pragma once

#include <stdint.h>
#include "display/cluster_pages.h"
#include "system/config_store.h"
#include "logging/csv_logger.h"
#include "obd/obd_client.h"

// Translates raw touch points (already calibrated/debounced by TouchManager)
// into cluster navigation and per-page control actions (config pages,
// diagnostics). Hit-test rectangles mirror the geometry cluster_pages.cpp
// draws, both sourced from display/cluster_layout.h so they cannot drift
// apart.
class ClusterTouchHandler {
public:
    ClusterTouchHandler(ClusterPages& clusterPages, ConfigStore& configStore, CsvLogger& csvLogger,
                         ObdClient& obdClient);

    // Call once per new touch-down edge (not on every poll while held) with
    // the touch point. Returns true if the active page changed; the caller
    // must then re-read currentPage() and trigger ClusterPages::drawStatic().
    bool handleTap(uint16_t x, uint16_t y, uint32_t nowMs);

    ClusterPage currentPage() const { return currentPage_; }

private:
    bool handleHeaderTap(uint16_t x, uint16_t y);
    bool handleConfigUiTap(uint16_t x, uint16_t y, uint32_t nowMs);
    bool handleConfigGaugesTap(uint16_t x, uint16_t y, uint32_t nowMs);
    bool handleConfigUserVarsTap(uint16_t x, uint16_t y, uint32_t nowMs);
    bool handleConfigLogsTap(uint16_t x, uint16_t y, uint32_t nowMs);
    bool handleConfigFooterTap(uint16_t x, uint16_t y, uint32_t nowMs);
    bool handleDiagnosticsTap(uint16_t x, uint16_t y, uint32_t nowMs);
    bool handlePerformanceTap(uint16_t x, uint16_t y);

    static bool within(uint16_t x, uint16_t y, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

    ClusterPages& clusterPages_;
    ConfigStore& configStore_;
    CsvLogger& csvLogger_;
    ObdClient& obdClient_;
    ClusterPage currentPage_ = ClusterPage::PrimaryCluster;

    // Remembered per-group page so the mode-toggle button returns you to
    // where you left off in the other group instead of always resetting to
    // that group's first page.
    ClusterPage lastDashboardPage_ = ClusterPage::PrimaryCluster;
    ClusterPage lastConfigPage_ = ClusterPage::ConfigUi;
};
