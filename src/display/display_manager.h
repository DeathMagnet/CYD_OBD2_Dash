#pragma once

#include <stdint.h>
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include "app_config.h"

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager() = default;

    // Disallow copy
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;

    bool begin(bool flipped);
    void setBacklight(uint8_t brightness);
    void clear(uint16_t color = TFT_BLACK);

    void drawBootImage();

    TFT_eSPI& getTft() { return tft_; }

private:
    static int pngDrawCallback(PNGDRAW* pDraw);

    TFT_eSPI tft_;
    // Heap-allocated only for the duration of drawBootImage(): PNG's ~39KB
    // internal zlib window/pixel buffers are only needed once at boot, well
    // before the Bluetooth stack and SD/touch subsystems claim heap, and
    // this board has no PSRAM to spare holding that buffer permanently as a
    // plain member would.
    PNG* png_ = nullptr;
};
