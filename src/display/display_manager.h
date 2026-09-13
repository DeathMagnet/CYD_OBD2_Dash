#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>
#include "app_config.h"

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager() = default;

    // Disallow copy
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    bool begin();
    void setBacklight(uint8_t brightness);
    void clear(uint16_t color = TFT_BLACK);

    void drawBootImage();

    TFT_eSPI& getTft() { return tft_; }

private:
    TFT_eSPI tft_;
};
