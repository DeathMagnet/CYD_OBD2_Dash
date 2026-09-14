#include "display/display_manager.h"
#include "assets/boot_logo_png.h"
#include "labels.h"
#include <Arduino.h>

DisplayManager::DisplayManager()
    : tft_() {
}

bool DisplayManager::begin() {
    Serial.println("[Display] Initializing ST7796S TFT display...");
    
    // Set up backlight pin
    pinMode(config::kTftBacklightPin, OUTPUT);
    digitalWrite(config::kTftBacklightPin, LOW); // Start dark to prevent boot flicker

    tft_.init();
    tft_.setRotation(config::kDisplayRotation);
    tft_.fillScreen(TFT_BLACK);

    // Turn backlight on
    setBacklight(255);
    Serial.printf("[Display] Display initialized. Resolution: %d x %d\n", tft_.width(), tft_.height());
    return true;
}

void DisplayManager::setBacklight(uint8_t brightness) {
    if (brightness > 0) {
        digitalWrite(config::kTftBacklightPin, HIGH);
    } else {
        digitalWrite(config::kTftBacklightPin, LOW);
    }
}

void DisplayManager::clear(uint16_t color) {
    tft_.fillScreen(color);
}

void DisplayManager::drawBootImage() {
    // The boot logo is stored as a losslessly-compressed indexed PNG
    // (kBootLogoPng, ~72KB) rather than the ~300KB raw RGB565 array it used
    // to be, and is decoded here one scanline at a time via PNGdec. Standard
    // (non-byte-swapped) RGB565 values are requested from getLineAsRGB565,
    // matching the same "standard" register order the raw array used to
    // hold, so setSwapBytes(true) still applies here for the same reason it
    // did before: pushImage only emits the byte order this panel expects
    // when swap is enabled.
    tft_.setSwapBytes(true);
    png_ = new PNG();
    int rc = png_->openRAM(const_cast<uint8_t*>(kBootLogoPng), kBootLogoPngSize, pngDrawCallback);
    if (rc == PNG_SUCCESS) {
        png_->decode(this, 0);
        png_->close();
    } else {
        Serial.printf("[Display] Boot logo PNG open failed: %d\n", rc);
    }
    delete png_;
    png_ = nullptr;
    Serial.println("[Display] Static boot screen rendered.");
}

int DisplayManager::pngDrawCallback(PNGDRAW* pDraw) {
    DisplayManager* self = static_cast<DisplayManager*>(pDraw->pUser);
    uint16_t lineBuffer[config::kScreenWidth];
    self->png_->getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
    self->tft_.pushImage(0, pDraw->y, pDraw->iWidth, 1, lineBuffer);
    return 1;
}
