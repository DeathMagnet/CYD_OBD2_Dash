#pragma once

#include <stdint.h>
#include <stddef.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/semphr.h>
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

    void lock() const;
    void unlock() const;

    bool loadTouchCalibration(uint16_t calData[config::kTouchCalDataSize]);
    bool saveTouchCalibration(const uint16_t calData[config::kTouchCalDataSize]);
    bool deleteTouchCalibration();

private:
    SPIClass sdSpiBus_;
    mutable SemaphoreHandle_t sdMutex_ = nullptr;
    bool isMounted_ = false;
};
