#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>
#include "app_config.h"
#include "storage/sd_manager.h"

class TouchManager {
public:
    TouchManager(TFT_eSPI& tft, SdManager& sdManager);
    ~TouchManager() = default;

    // Disallow copy
    TouchManager(const TouchManager&) = delete;
    TouchManager& operator=(const TouchManager&) = delete;

    bool begin();
    bool getTouch(uint16_t& x, uint16_t& y);
    void calibrateAndSave();

private:
    TFT_eSPI& tft_;
    SdManager& sdManager_;
    uint16_t calData_[config::kTouchCalDataSize] = {0};
    bool isCalibrated_ = false;
};
