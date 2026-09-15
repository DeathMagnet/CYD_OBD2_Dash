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

    // Simple text-only fatal error screen for unrecoverable boot-time
    // failures (e.g. a missing/invalid /obd_config.txt): fills the screen
    // black, draws `title` centered near the top in warning red, and the two
    // detail lines centered below it in white. Uses TFT_eSPI's built-in font
    // directly rather than the theme engine, so it renders even if theme/
    // label setup is the thing that failed. Callers are expected to halt
    // afterward - this does not return control anywhere meaningful.
    void showFatalError(const char* title, const char* line1, const char* line2);

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
