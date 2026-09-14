#include "storage/sd_manager.h"
#include <Arduino.h>

SdManager::SdManager()
    : sdSpiBus_(VSPI) {
}

bool SdManager::begin() {
    Serial.println("[SD] Initializing VSPI bus for SD card...");
    sdSpiBus_.begin(config::kSdSpiClkPin, config::kSdSpiMisoPin, config::kSdSpiMosiPin, config::kSdSpiCsPin);

    if (!SD.begin(config::kSdSpiCsPin, sdSpiBus_, config::kSdSpiFrequency)) {
        Serial.println("[SD] Failed to mount SD card!");
        isMounted_ = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No SD card attached.");
        isMounted_ = false;
        return false;
    }

    uint64_t cardSizeMb = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card mounted successfully. Size: %llu MB\n", cardSizeMb);
    isMounted_ = true;
    return true;
}

bool SdManager::loadTouchCalibration(uint16_t calData[config::kTouchCalDataSize]) {
    if (!isMounted_) {
        Serial.println("[SD] Cannot read calibration: SD not mounted");
        return false;
    }

    if (!SD.exists(config::kTouchCalFilePath)) {
        Serial.printf("[SD] Touch calibration file '%s' does not exist.\n", config::kTouchCalFilePath);
        return false;
    }

    File file = SD.open(config::kTouchCalFilePath, FILE_READ);
    if (!file) {
        Serial.printf("[SD] Failed to open '%s' for reading.\n", config::kTouchCalFilePath);
        return false;
    }

    size_t expectedBytes = config::kTouchCalDataSize * sizeof(uint16_t);
    size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(calData), expectedBytes);
    file.close();

    if (bytesRead != expectedBytes) {
        Serial.printf("[SD] Invalid/corrupt calibration file (read %u of %u bytes).\n",
                      static_cast<unsigned int>(bytesRead),
                      static_cast<unsigned int>(expectedBytes));
        return false;
    }

    Serial.printf("[SD] Loaded touch calibration: [%u, %u, %u, %u, %u]\n",
                  calData[0], calData[1], calData[2], calData[3], calData[4]);
    return true;
}

bool SdManager::saveTouchCalibration(const uint16_t calData[config::kTouchCalDataSize]) {
    if (!isMounted_) {
        Serial.println("[SD] Cannot save calibration: SD not mounted");
        return false;
    }

    File file = SD.open(config::kTouchCalFilePath, FILE_WRITE);
    if (!file) {
        Serial.printf("[SD] Failed to open '%s' for writing.\n", config::kTouchCalFilePath);
        return false;
    }

    size_t expectedBytes = config::kTouchCalDataSize * sizeof(uint16_t);
    size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(calData), expectedBytes);
    file.flush();
    file.close();

    if (bytesWritten != expectedBytes) {
        Serial.printf("[SD] Failed to write complete calibration data (%u of %u bytes).\n",
                      static_cast<unsigned int>(bytesWritten),
                      static_cast<unsigned int>(expectedBytes));
        return false;
    }

    Serial.printf("[SD] Saved touch calibration to '%s': [%u, %u, %u, %u, %u]\n",
                  config::kTouchCalFilePath,
                  calData[0], calData[1], calData[2], calData[3], calData[4]);
    return true;
}

bool SdManager::deleteTouchCalibration() {
    if (!isMounted_) {
        Serial.println("[SD] Cannot delete calibration: SD not mounted");
        return false;
    }

    if (!SD.exists(config::kTouchCalFilePath)) {
        return true;
    }

    if (!SD.remove(config::kTouchCalFilePath)) {
        Serial.printf("[SD] Failed to delete '%s'.\n", config::kTouchCalFilePath);
        return false;
    }

    Serial.printf("[SD] Deleted touch calibration file '%s'.\n", config::kTouchCalFilePath);
    return true;
}
