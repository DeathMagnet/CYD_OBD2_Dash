#pragma once

#include <stdint.h>
#include <stddef.h>

constexpr uint8_t kMaxDtcCount = 12;
constexpr uint8_t kDtcCodeLength = 6; // e.g. "P0133" + null terminator

struct DtcList {
    char codes[kMaxDtcCount][kDtcCodeLength] = {};
    uint8_t count = 0;
};

// Decodes one raw ELM327 response line from Mode 03 (stored codes) or Mode 07
// (pending codes) into human-readable DTC strings (e.g. "P0133"), appending
// to `out`. Best-effort: this project does not reassemble multi-frame ISO-TP
// beyond what the ELM327 already flattens into one line, and a stray leading
// DTC-count byte is inferred rather than guaranteed. modeAckByte is 0x43 for
// Mode 03 or 0x47 for Mode 07.
void decodeDtcResponseLine(const char* line, uint8_t modeAckByte, DtcList& out);
