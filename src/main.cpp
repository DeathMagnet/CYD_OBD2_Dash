#include <Arduino.h>
#include "app_config.h"
#include "display/display_manager.h"
#include "storage/sd_manager.h"
#include "input/touch_manager.h"

// Subsystem instances
static DisplayManager displayManager;
static SdManager sdManager;
static TouchManager touchManager(displayManager.getTft(), sdManager);

// State variables
static uint32_t lastTouchPollMs = 0;
static bool lastTouchState = false;
static uint16_t lastTouchX = 0;
static uint16_t lastTouchY = 0;

void setup() {
    Serial.begin(115200);
    delay(200); // Brief power stabilization
    Serial.println("\n==========================================");
    Serial.println("   CYD 4.0\" ESP32-32E OBD-II Dashboard    ");
    Serial.println("==========================================");

    // 1. Initialize Display & Backlight
    displayManager.begin();

    // 2. Render Static Boot Screen from native RGB565 PROGMEM array
    Serial.println("[Boot] Displaying static boot image...");
    displayManager.drawBootImage();

    // Hold boot screen for configured duration (2 seconds)
    delay(config::kBootScreenDurationMs);

    // 3. Initialize SD Card over VSPI
    sdManager.begin();

    // 4. Initialize Touch & Run Calibration if missing from SD
    touchManager.begin();

    // 5. Render "Hello, World" initial view
    displayManager.clear(TFT_BLACK);
    displayManager.drawHelloWorld(0, 0, false);
}

void loop() {
    uint32_t nowMs = millis();

    // Non-blocking touch polling
    if (nowMs - lastTouchPollMs >= config::kTouchPollIntervalMs) {
        lastTouchPollMs = nowMs;

        uint16_t touchX = 0;
        uint16_t touchY = 0;
        bool isTouched = touchManager.getTouch(touchX, touchY);

        // Update display if touch state or position changes
        if (isTouched) {
            lastTouchX = touchX;
            lastTouchY = touchY;
            lastTouchState = true;
            displayManager.drawHelloWorld(touchX, touchY, true);
        } else if (lastTouchState) {
            lastTouchState = false;
            displayManager.drawHelloWorld(lastTouchX, lastTouchY, false);
        }
    }
}
