#pragma once

#include <stdint.h>
#include "labels.h"

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
        case ConnectionState::Boot: return labels::kStatusBoot;
        case ConnectionState::DisplayReady: return labels::kStatusDisplayReady;
        case ConnectionState::SdInit: return labels::kStatusSdInit;
        case ConnectionState::ObdConnecting: return labels::kStatusObdConnecting;
        case ConnectionState::Live: return labels::kStatusLive;
        case ConnectionState::Stale: return labels::kStatusStale;
        case ConnectionState::Reconnecting: return labels::kStatusReconnecting;
        case ConnectionState::Degraded: return labels::kStatusDegraded;
    }
    return labels::kStatusUnknown;
}
