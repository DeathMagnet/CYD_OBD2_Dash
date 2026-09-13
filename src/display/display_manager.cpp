#include "display/display_manager.h"
#include "assets/boot_0_rgb565.h"
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
    // kBoot0Rgb565 stores standard (non-byte-swapped) RGB565 values; pushPixels only
    // emits the byte order this panel expects when swap is enabled (fillScreen's
    // pushBlock path does this swap internally, which is why solid fills look correct
    // even when this flag is wrong).
    tft_.setSwapBytes(true);
    tft_.pushImage(0, 0, config::kScreenWidth, config::kScreenHeight, kBoot0Rgb565);
    Serial.println("[Display] Static boot screen rendered.");
}
