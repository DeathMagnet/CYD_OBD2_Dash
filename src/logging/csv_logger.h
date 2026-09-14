#pragma once

#include <stdint.h>
#include <stddef.h>
#include <SD.h>
#include "app_config.h"
#include "storage/sd_manager.h"
#include "obd/telemetry.h"

struct LogSummary {
    uint32_t fileCount = 0;
    uint64_t totalBytes = 0;
};

// Owns SD-card CSV telemetry logging: one session file per boot (see
// docs/cyd-obd2-sd-logging-guide.md for the 20-column schema), a
// user-configurable row interval (Logs config page), periodic buffered flushes, and
// automatic pruning of the oldest session logs when the card runs low on
// space so the active session never fails to write for lack of room.
class CsvLogger {
public:
    explicit CsvLogger(SdManager& sdManager);
    ~CsvLogger() = default;

    CsvLogger(const CsvLogger&) = delete;
    CsvLogger& operator=(const CsvLogger&) = delete;

    // Prunes old logs if the card is near capacity, then opens a new
    // auto-incrementing session file. No-ops safely if SD is not mounted.
    bool begin();

    // Call every loop() iteration. Writes one row at rowIntervalMs cadence
    // and flushes at config::kSdFlushIntervalMs cadence; a flush also
    // re-checks free space and prunes if needed. Missing telemetry fields
    // are written as empty CSV cells, never fabricated zeros.
    void update(const TelemetrySnapshot& snapshot, uint32_t nowMs, uint32_t rowIntervalMs);

    void flush();
    void end();

    bool isLoggingActive() const { return loggingActive_; }
    const char* currentFilePath() const { return currentFilePath_; }

    // Scans the SD root for all session log files.
    LogSummary getLogSummary() const;

    // Closes the active file, deletes every session log file, then reopens a
    // fresh session file only if logging was already active. Returns files deleted.
    uint32_t deleteAllLogs();

    // Sets whether CSV logging should use metric or standard units
    void setUnitsMetric(bool metric) { useMetricLogs_ = metric; }

private:
    using LogFileVisitor = void (*)(void* context, const char* path, uint32_t sessionIndex, size_t fileSizeBytes);

    bool openNewSessionFile();
    void enforceCapacity();
    void writeHeader();
    void forEachLogFile(LogFileVisitor visitor, void* context) const;

    SdManager& sdManager_;
    File logFile_;
    char currentFilePath_[32] = {0};
    bool loggingActive_ = false;
    uint32_t lastRowMs_ = 0;
    uint32_t lastFlushMs_ = 0;
    bool useMetricLogs_ = false;
};
