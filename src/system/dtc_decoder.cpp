#include "system/dtc_decoder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace {

void appendDtc(DtcList& out, uint8_t highByte, uint8_t lowByte) {
    if (highByte == 0 && lowByte == 0) {
        return; // Padding, not a real code.
    }
    if (out.count >= kMaxDtcCount) {
        return;
    }

    static const char kTypeChars[4] = {'P', 'C', 'B', 'U'};
    uint8_t type = (highByte >> 6) & 0x03;
    uint8_t digit1 = (highByte >> 4) & 0x03;
    uint8_t digit2 = highByte & 0x0F;
    uint8_t digit3 = (lowByte >> 4) & 0x0F;
    uint8_t digit4 = lowByte & 0x0F;

    snprintf(out.codes[out.count], kDtcCodeLength, "%c%01X%01X%01X%01X",
             kTypeChars[type], digit1, digit2, digit3, digit4);
    out.count++;
}

} // namespace

void decodeDtcResponseLine(const char* line, uint8_t modeAckByte, DtcList& out) {
    if (line == nullptr || line[0] == '\0') {
        return;
    }

    char buf[80];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buf, " \r\n", &savePtr);

    uint8_t bytes[40];
    uint8_t byteCount = 0;
    while (token != nullptr && byteCount < sizeof(bytes)) {
        char* endPtr = nullptr;
        long parsed = strtol(token, &endPtr, 16);
        if (endPtr == token || *endPtr != '\0' || parsed < 0 || parsed > 0xFF) {
            return; // Non-hex noise: "SEARCHING...", "NO DATA", prompts, etc.
        }
        bytes[byteCount++] = static_cast<uint8_t>(parsed);
        token = strtok_r(nullptr, " \r\n", &savePtr);
    }

    if (byteCount == 0) {
        return;
    }

    size_t startIdx = (bytes[0] == modeAckByte) ? 1 : 0;
    size_t remaining = byteCount - startIdx;
    // A leading DTC-count byte leaves an odd remainder; skip it so the walk
    // below stays aligned on 2-byte DTC pairs.
    size_t offset = startIdx + (remaining % 2);
    size_t pairCount = (remaining - (remaining % 2)) / 2;

    for (size_t i = 0; i < pairCount; ++i) {
        appendDtc(out, bytes[offset + i * 2], bytes[offset + i * 2 + 1]);
    }
}
