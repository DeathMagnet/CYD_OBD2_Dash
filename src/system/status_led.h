#pragma once

#include <stdint.h>

// Drives the board's onboard RGB LED: steady red while Check Engine (MIL) is
// on, flashing the active theme's warning color (same cadence as the Page-1
// shift-light text flash) while the shift light is active, off otherwise.
class StatusLed {
public:
    StatusLed() = default;
    ~StatusLed() = default;

    // Disallow copy
    StatusLed(const StatusLed&) = delete;
    StatusLed& operator=(const StatusLed&) = delete;

    void begin();
    void update(bool checkEngineOn, bool shiftLightOn, uint16_t warningColorRgb565, uint32_t nowMs);
};
