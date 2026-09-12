#pragma once

#include <stdint.h>
#include <stddef.h>
#include <SPI.h>
#include <SD.h>
#include "app_config.h"

class SdManager {
public:
    SdManager();
    ~SdManager() = default;

    // Disallow copy
    SdManager(const SdManager&) = delete;
    SdManager& operator=(const SdManager&) = delete;

    bool begin();
    bool isMounted() const { return isMounted_; }

    bool loadTouchCalibration(uint16_t calData[config::kTouchCalDataSize]);
    bool saveTouchCalibration(const uint16_t calData[config::kTouchCalDataSize]);

private:
    SPIClass sdSpiBus_;
    bool isMounted_ = false;
};
