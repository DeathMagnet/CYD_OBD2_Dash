#include "system/status_led.h"
#include <Arduino.h>
#include "app_config.h"

namespace {
// Active-low: LOW turns a channel on, HIGH turns it off.
constexpr uint8_t kLedOn = LOW;
constexpr uint8_t kLedOff = HIGH;
} // namespace

void StatusLed::begin() {
    pinMode(config::kLedRedPin, OUTPUT);
    pinMode(config::kLedGreenPin, OUTPUT);
    pinMode(config::kLedBluePin, OUTPUT);
    digitalWrite(config::kLedRedPin, kLedOff);
    digitalWrite(config::kLedGreenPin, kLedOff);
    digitalWrite(config::kLedBluePin, kLedOff);
}

void StatusLed::update(bool checkEngineOn, bool shiftLightOn, uint16_t warningColorRgb565, uint32_t nowMs) {
    bool flashPhase = ((nowMs / config::kLedFlashIntervalMs) % 2) == 0;

    if (shiftLightOn && flashPhase) {
        // Quantize the theme's RGB565 warning color down to the LED's 3
        // digital (non-PWM) channels, each thresholded at half-scale.
        uint8_t r5 = (warningColorRgb565 >> 11) & 0x1F;
        uint8_t g6 = (warningColorRgb565 >> 5) & 0x3F;
        uint8_t b5 = warningColorRgb565 & 0x1F;
        digitalWrite(config::kLedRedPin, r5 >= 16 ? kLedOn : kLedOff);
        digitalWrite(config::kLedGreenPin, g6 >= 32 ? kLedOn : kLedOff);
        digitalWrite(config::kLedBluePin, b5 >= 16 ? kLedOn : kLedOff);
    } else if (checkEngineOn) {
        digitalWrite(config::kLedRedPin, kLedOn);
        digitalWrite(config::kLedGreenPin, kLedOff);
        digitalWrite(config::kLedBluePin, kLedOff);
    } else {
        digitalWrite(config::kLedRedPin, kLedOff);
        digitalWrite(config::kLedGreenPin, kLedOff);
        digitalWrite(config::kLedBluePin, kLedOff);
    }
}
