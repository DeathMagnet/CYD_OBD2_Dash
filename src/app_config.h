#pragma once

#include <stdint.h>
#include <stddef.h>

namespace config {

// Display Configuration (ST7796S 4.0-inch CYD)
constexpr uint16_t kScreenWidth = 480;
constexpr uint16_t kScreenHeight = 320;
constexpr uint8_t kDisplayRotation = 1; // Landscape (480x320)
constexpr uint8_t kTftBacklightPin = 27;

// Boot Screen Timing
constexpr uint32_t kBootScreenDurationMs = 2000;

// SD Card Pins (VSPI)
constexpr uint8_t kSdSpiCsPin = 5;
constexpr uint8_t kSdSpiMosiPin = 23;
constexpr uint8_t kSdSpiMisoPin = 19;
constexpr uint8_t kSdSpiClkPin = 18;
constexpr uint32_t kSdSpiFrequency = 20000000; // 20 MHz

// Touch Pins (Hosyond 4.0" CYD XPT2046 over TFT SPI)
constexpr uint8_t kTouchCsPin   = 33;

// Touch Calibration File & Array Size
constexpr const char* kTouchCalFilePath = "/touch_cal.dat";
constexpr size_t kTouchCalDataSize = 5;

// Touch Polling Interval & Pressure Threshold
constexpr uint32_t kTouchPollIntervalMs = 20;
constexpr uint16_t kTouchPressureThreshold = 200;

} // namespace config
