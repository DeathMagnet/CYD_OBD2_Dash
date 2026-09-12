#include "input/touch_manager.h"
#include <Arduino.h>

TouchManager::TouchManager(TFT_eSPI& tft, SdManager& sdManager)
    : tft_(tft), sdManager_(sdManager) {
}

bool TouchManager::begin() {
    Serial.println("[Touch] Checking touch calibration status...");

    // Try loading calibration from SD card
    if (sdManager_.isMounted() && sdManager_.loadTouchCalibration(calData_)) {
        tft_.setTouch(calData_);
        isCalibrated_ = true;
        Serial.println("[Touch] Touch calibration loaded from SD card.");
        return true;
    }

    // If missing from SD, run screen calibration
    Serial.println("[Touch] Touch calibration not found on SD. Running calibration routine...");
    calibrateAndSave();
    return true;
}

void TouchManager::calibrateAndSave() {
    tft_.fillScreen(TFT_BLACK);
    tft_.setTextColor(TFT_WHITE, TFT_BLACK);
    tft_.setTextSize(2);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("Touch Calibration", 240, 100);
    tft_.drawString("Touch the corners firmly", 240, 140);

    // Run TFT_eSPI 4-point corner calibration
    Serial.println("[Touch] Starting touch calibration...");
    tft_.calibrateTouch(calData_, TFT_RED, TFT_BLACK, 15);

    tft_.setTouch(calData_);
    isCalibrated_ = true;

    Serial.printf("[Touch] Calibration complete: [%u, %u, %u, %u, %u]\n",
                  calData_[0], calData_[1], calData_[2], calData_[3], calData_[4]);

    // Save to SD card if available
    if (sdManager_.isMounted()) {
        if (sdManager_.saveTouchCalibration(calData_)) {
            Serial.println("[Touch] Calibration persisted to SD card successfully.");
        } else {
            Serial.println("[Touch] Warning: Failed to persist calibration to SD card.");
        }
    } else {
        Serial.println("[Touch] Warning: SD card not mounted. Calibration not persisted.");
    }
    tft_.fillScreen(TFT_BLACK);
}

bool TouchManager::getTouch(uint16_t& x, uint16_t& y) {
    return tft_.getTouch(&x, &y, config::kTouchPressureThreshold);
}
