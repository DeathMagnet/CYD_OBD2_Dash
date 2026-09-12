#pragma once

#include <stdint.h>

// Shared lifecycle used by main.cpp, the OBD client, and the renderer.
// ObdClient only ever reports Connecting/Live/Reconnecting for itself; Stale
// is a presentation-time judgement (main.cpp compares snapshot freshness
// against config::kTelemetryStaleThresholdMs) rather than transport state.
enum class ConnectionState : uint8_t {
    Boot,
    DisplayReady,
    SdInit,
    ObdConnecting,
    Live,
    Stale,
    Reconnecting,
    Degraded,
};

inline const char* toString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Boot: return "BOOT";
        case ConnectionState::DisplayReady: return "DISPLAY READY";
        case ConnectionState::SdInit: return "SD INIT";
        case ConnectionState::ObdConnecting: return "CONNECTING";
        case ConnectionState::Live: return "LIVE";
        case ConnectionState::Stale: return "STALE";
        case ConnectionState::Reconnecting: return "RECONNECTING";
        case ConnectionState::Degraded: return "NO OBD";
    }
    return "UNKNOWN";
}
