#include "display/display_manager.h"
#include "assets/boot_0_rgb565.h"
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

void DisplayManager::drawHelloWorld(uint16_t touchX, uint16_t touchY, bool isTouched) {
    // Header Bar
    tft_.fillRect(0, 0, 480, 40, 0x0821); // Midnight blue
    tft_.drawFastHLine(0, 40, 480, 0x051D); // Cyan accent line

    tft_.setTextColor(0x051D, 0x0821); // Cyan text
    tft_.setTextSize(2);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("CYD OBD-II DASHBOARD", 240, 20);

    // Main Card Frame
    tft_.fillRoundRect(40, 60, 400, 180, 8, 0x18C3); // Slate frame
    tft_.drawRoundRect(40, 60, 400, 180, 8, 0xC618); // Silver border

    // Hello World Title
    tft_.setTextColor(TFT_WHITE, 0x18C3);
    tft_.setTextSize(3);
    tft_.drawString("Hello, World!", 240, 110);

    tft_.setTextColor(0x07E0, 0x18C3); // Neon Green
    tft_.setTextSize(2);
    tft_.drawString("Touch Calibration Active", 240, 160);

    // Touch readout box
    tft_.fillRect(40, 260, 400, 45, TFT_BLACK);
    tft_.drawRect(40, 260, 400, 45, 0x051D);

    tft_.setTextSize(2);
    char buf[64];
    if (isTouched) {
        snprintf(buf, sizeof(buf), "Touch: X=%-3u  Y=%-3u", touchX, touchY);
        tft_.setTextColor(0x07FF, TFT_BLACK); // Cyan
        tft_.drawString(buf, 240, 282);

        // Draw crosshair marker at touched position
        tft_.drawCircle(touchX, touchY, 6, TFT_RED);
        tft_.drawCircle(touchX, touchY, 7, TFT_WHITE);
    } else {
        tft_.setTextColor(0x7BEF, TFT_BLACK); // Gray
        tft_.drawString("Touch anywhere on screen", 240, 282);
    }
}
