#include "obd/obd_pairing.h"

// Bench/demo builds never touch a real Bluetooth adapter (see
// obd/obd_client.h, which defines this same flag for the rest of the OBD
// module); this whole file compiles out under OBD_SIMULATION_ENABLED so the
// simulation build keeps its ~770KB Bluetooth-stack flash savings.
#ifndef OBD_SIMULATION_ENABLED
#define OBD_SIMULATION_ENABLED 0
#endif

#if !OBD_SIMULATION_ENABLED

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <TFT_eSPI.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include "app_config.h"
#include "labels.h"
#include "display/display_manager.h"
#include "display/theme.h"
#include "input/touch_manager.h"
#include "storage/sd_manager.h"
#include "obd/obd_credentials.h"
#include "logging/connection_logger.h"

namespace obd_pairing {

namespace {

struct DiscoveredDevice {
    char name[32];
    uint8_t mac[6];
};

struct PairingSession {
    BluetoothSerial btSerial;
    DiscoveredDevice devices[config::kPairingMaxDevices];
    size_t deviceCount = 0;
    size_t deviceIndex = 0;
    size_t passwordIndex = 0;
    uint8_t failedAttempts = 0;
};

// Layout: header title + 3 stacked full-width buttons + a status line,
// matching the 20px side-margin convention used by the Save button in
// display/cluster_layout.h.
constexpr int32_t kButtonX = 20;
constexpr int32_t kButtonW = config::kScreenWidth - 2 * kButtonX;
constexpr int32_t kButtonH = 60;
constexpr int32_t kButtonGap = 15;
constexpr int32_t kDeviceButtonY = 55;
constexpr int32_t kPasswordButtonY = kDeviceButtonY + kButtonH + kButtonGap;
constexpr int32_t kConnectButtonY = kPasswordButtonY + kButtonH + kButtonGap;
constexpr int32_t kStatusLineY = 285;

bool within(uint16_t x, uint16_t y, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    return static_cast<int32_t>(x) >= x0 && static_cast<int32_t>(x) < x1 &&
           static_cast<int32_t>(y) >= y0 && static_cast<int32_t>(y) < y1;
}

bool containsIgnoreCase(const char* haystack, const char* needle) {
    size_t haystackLen = strlen(haystack);
    size_t needleLen = strlen(needle);
    if (needleLen == 0 || needleLen > haystackLen) {
        return false;
    }
    for (size_t i = 0; i + needleLen <= haystackLen; ++i) {
        size_t j = 0;
        for (; j < needleLen; ++j) {
            if (tolower(static_cast<unsigned char>(haystack[i + j])) !=
                tolower(static_cast<unsigned char>(needle[j]))) {
                break;
            }
        }
        if (j == needleLen) {
            return true;
        }
    }
    return false;
}

bool matchesKnownAdapterPattern(const char* name) {
    for (size_t i = 0; i < config::kPairingKnownAdapterPatternCount; ++i) {
        if (containsIgnoreCase(name, config::kPairingKnownAdapterPatterns[i])) {
            return true;
        }
    }
    return false;
}

// Blocking classic-BT inquiry (BluetoothSerial::discover()). Filters results
// to names matching a known OBD-adapter pattern, falling back to the full
// list if nothing matches. Returns the number of entries written into `out`
// (capped at config::kPairingMaxDevices).
size_t discoverObdDevices(BluetoothSerial& btSerial, DiscoveredDevice* out) {
    BTScanResults* results = btSerial.discover(config::kPairingScanTimeoutMs);
    if (results == nullptr) {
        return 0;
    }

    DiscoveredDevice rawDevices[config::kPairingMaxDevices];
    size_t rawCount = 0;
    int count = results->getCount();
    for (int i = 0; i < count && rawCount < config::kPairingMaxDevices; ++i) {
        BTAdvertisedDevice* device = results->getDevice(i);
        if (device == nullptr || !device->haveName()) {
            continue;
        }
        std::string name = device->getName();
        if (name.empty()) {
            continue;
        }
        strncpy(rawDevices[rawCount].name, name.c_str(), sizeof(rawDevices[rawCount].name) - 1);
        rawDevices[rawCount].name[sizeof(rawDevices[rawCount].name) - 1] = '\0';
        BTAddress address = device->getAddress();
        memcpy(rawDevices[rawCount].mac, *address.getNative(), sizeof(rawDevices[rawCount].mac));
        rawCount++;
    }

    size_t filteredCount = 0;
    for (size_t i = 0; i < rawCount; ++i) {
        if (matchesKnownAdapterPattern(rawDevices[i].name)) {
            out[filteredCount++] = rawDevices[i];
        }
    }
    if (filteredCount > 0) {
        return filteredCount;
    }

    for (size_t i = 0; i < rawCount; ++i) {
        out[i] = rawDevices[i];
    }
    return rawCount;
}

void drawButton(TFT_eSPI& tft, const ThemeColors& theme, int32_t y, const char* text, uint16_t fill) {
    tft.fillRoundRect(kButtonX, y, kButtonW, kButtonH, 6, fill);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(theme.textPrimary, fill);
    tft.drawString(text, config::kScreenWidth / 2, y + kButtonH / 2);
}

void drawTitle(TFT_eSPI& tft, const ThemeColors& theme) {
    tft.fillRect(0, 0, config::kScreenWidth, config::kHeaderHeight, theme.panel);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(theme.textPrimary, theme.panel);
    tft.drawString(labels::kPairingTitle, config::kScreenWidth / 2, config::kHeaderHeight / 2);
}

void drawDeviceButton(TFT_eSPI& tft, const ThemeColors& theme, const PairingSession& session) {
    if (session.deviceCount == 0) {
        drawButton(tft, theme, kDeviceButtonY, labels::kPairingDeviceEmpty, theme.panel);
        return;
    }
    char label[48];
    snprintf(label, sizeof(label), "%s%s", labels::kPairingDevicePrefix, session.devices[session.deviceIndex].name);
    drawButton(tft, theme, kDeviceButtonY, label, theme.panel);
}

void drawPasswordButton(TFT_eSPI& tft, const ThemeColors& theme, const PairingSession& session) {
    char label[48];
    snprintf(label, sizeof(label), "%s%s", labels::kPairingPasswordPrefix,
              config::kPairingPasswordOptions[session.passwordIndex]);
    drawButton(tft, theme, kPasswordButtonY, label, theme.panel);
}

void drawConnectButton(TFT_eSPI& tft, const ThemeColors& theme, bool busy) {
    drawButton(tft, theme, kConnectButtonY, busy ? labels::kStatusConnecting : labels::kButtonConnect,
               theme.primaryGaugeArc);
}

void drawStatusLine(TFT_eSPI& tft, const ThemeColors& theme, const char* message) {
    tft.fillRect(0, kStatusLineY - 5, config::kScreenWidth, 30, theme.background);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(theme.textSecondary, theme.background);
    tft.drawString(message, config::kScreenWidth / 2, kStatusLineY + 10);
}

void rescanDevices(TFT_eSPI& tft, const ThemeColors& theme, SdManager& sdManager, PairingSession& session) {
    drawStatusLine(tft, theme, labels::kStatusPairingScanning);
    connection_log::write(sdManager, "Pairing: scanning for Bluetooth devices");

    session.deviceCount = discoverObdDevices(session.btSerial, session.devices);
    session.deviceIndex = 0;
    drawDeviceButton(tft, theme, session);

    if (session.deviceCount > 0) {
        char status[64];
        snprintf(status, sizeof(status), "Found %u device(s).", static_cast<unsigned>(session.deviceCount));
        drawStatusLine(tft, theme, status);
    } else {
        drawStatusLine(tft, theme, labels::kStatusPairingScanEmpty);
    }
    connection_log::writef(sdManager, "Pairing: scan found %u device(s)",
                            static_cast<unsigned>(session.deviceCount));
}

bool attemptConnect(BluetoothSerial& btSerial, const uint8_t mac[6], const char* password) {
    uint8_t macCopy[6];
    memcpy(macCopy, mac, sizeof(macCopy));
    btSerial.setPin(password);
    bool connected = btSerial.connect(macCopy);
    if (!connected) {
        btSerial.disconnect();
    }
    return connected;
}

} // namespace

bool preflight(const ObdCredentials& creds) {
    if (!creds.hasMac) {
        return false;
    }

    BluetoothSerial btSerial;
    btSerial.begin("CYD_OBD_Dash", true);

    uint8_t macCopy[6];
    memcpy(macCopy, creds.mac, sizeof(macCopy));

    bool connected = false;
    for (uint8_t attempt = 0; attempt < config::kPreflightMaxAttempts && !connected; ++attempt) {
        if (attempt > 0) {
            delay(500);
        }
        Serial.printf("[Pairing] Preflight attempt %u/%u against stored credentials...\n", attempt + 1,
                      config::kPreflightMaxAttempts);
        connected = attemptConnect(btSerial, macCopy, creds.password);
    }

    if (connected) {
        btSerial.disconnect();
    }
    btSerial.end();
    return connected;
}

void run(DisplayManager& displayManager, TouchManager& touchManager, SdManager& sdManager,
         const ThemeColors& theme, ObdCredentials& out) {
    TFT_eSPI& tft = displayManager.getTft();
    connection_log::write(sdManager, "Pairing: entering Bluetooth pairing screen");

    PairingSession session;
    session.btSerial.begin("CYD_OBD_Dash", true);

    tft.fillScreen(theme.background);
    drawTitle(tft, theme);
    rescanDevices(tft, theme, sdManager, session);
    drawPasswordButton(tft, theme, session);
    drawConnectButton(tft, theme, false);

    bool lastTouchState = false;
    uint32_t lastTouchPollMs = 0;

    for (;;) {
        uint32_t nowMs = millis();
        if (nowMs - lastTouchPollMs < config::kTouchPollIntervalMs) {
            delay(1);
            continue;
        }
        lastTouchPollMs = nowMs;

        uint16_t touchX = 0;
        uint16_t touchY = 0;
        bool isTouched = touchManager.getTouch(touchX, touchY);
        bool tapped = isTouched && !lastTouchState;
        lastTouchState = isTouched;
        if (!tapped) {
            continue;
        }

        if (within(touchX, touchY, kButtonX, kDeviceButtonY, kButtonX + kButtonW, kDeviceButtonY + kButtonH)) {
            if (session.deviceCount == 0) {
                rescanDevices(tft, theme, sdManager, session);
            } else {
                session.deviceIndex = (session.deviceIndex + 1) % session.deviceCount;
                drawDeviceButton(tft, theme, session);
            }
            continue;
        }

        if (within(touchX, touchY, kButtonX, kPasswordButtonY, kButtonX + kButtonW, kPasswordButtonY + kButtonH)) {
            session.passwordIndex = (session.passwordIndex + 1) % config::kPairingPasswordOptionCount;
            drawPasswordButton(tft, theme, session);
            continue;
        }

        if (!within(touchX, touchY, kButtonX, kConnectButtonY, kButtonX + kButtonW, kConnectButtonY + kButtonH)) {
            continue;
        }

        if (session.deviceCount == 0) {
            drawStatusLine(tft, theme, labels::kStatusPairingTapDeviceFirst);
            continue;
        }

        const DiscoveredDevice& device = session.devices[session.deviceIndex];
        const char* password = config::kPairingPasswordOptions[session.passwordIndex];

        ObdCredentials candidate;
        candidate.hasMac = true;
        memcpy(candidate.mac, device.mac, sizeof(candidate.mac));
        strncpy(candidate.id, device.name, sizeof(candidate.id) - 1);
        candidate.id[sizeof(candidate.id) - 1] = '\0';
        strncpy(candidate.password, password, sizeof(candidate.password) - 1);
        candidate.password[sizeof(candidate.password) - 1] = '\0';

        saveObdCredentials(sdManager, candidate);

        drawConnectButton(tft, theme, true);
        char connectingMsg[64];
        snprintf(connectingMsg, sizeof(connectingMsg), "Connecting to %s...", device.name);
        drawStatusLine(tft, theme, connectingMsg);
        connection_log::writef(sdManager, "Pairing: attempting connect to %s", device.name);

        bool connected = attemptConnect(session.btSerial, device.mac, password);

        if (connected) {
            connection_log::writef(sdManager, "Pairing: connected to %s; credentials saved", device.name);
            session.btSerial.disconnect();
            session.btSerial.end();
            out = candidate;
            return;
        }

        session.failedAttempts++;
        connection_log::writef(sdManager, "Pairing: connect failed (%u/%u)", session.failedAttempts,
                                config::kPairingMaxAttempts);
        drawConnectButton(tft, theme, false);

        char failMsg[64];
        if (session.failedAttempts >= config::kPairingMaxAttempts) {
            snprintf(failMsg, sizeof(failMsg), "FAILED %u/%u - rescanning...", session.failedAttempts,
                      config::kPairingMaxAttempts);
            drawStatusLine(tft, theme, failMsg);
            delay(config::kPairingStatusFlashMs);

            session.failedAttempts = 0;
            session.passwordIndex = 0;
            rescanDevices(tft, theme, sdManager, session);
            drawPasswordButton(tft, theme, session);
        } else {
            snprintf(failMsg, sizeof(failMsg), "FAILED (%u/%u) - try again", session.failedAttempts,
                      config::kPairingMaxAttempts);
            drawStatusLine(tft, theme, failMsg);
        }
    }
}

} // namespace obd_pairing

#endif // !OBD_SIMULATION_ENABLED
