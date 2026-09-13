#include "logging/csv_logger.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdlib.h>

namespace {

void formatIntField(char* buf, size_t bufSize, const TelemetryValue& value) {
    if (value.valid) {
        snprintf(buf, bufSize, "%d", static_cast<int>(value.value));
    } else {
        buf[0] = '\0';
    }
}

void formatFloatField(char* buf, size_t bufSize, const TelemetryValue& value) {
    if (value.valid) {
        snprintf(buf, bufSize, "%.2f", static_cast<double>(value.value));
    } else {
        buf[0] = '\0';
    }
}

// name may come back as "/mustang_log_007.csv" or "mustang_log_007.csv"
// depending on the ESP32 core version; normalize to a leading-slash path.
void buildPathFromName(const char* name, char* out, size_t outSize) {
    if (name[0] == '/') {
        strncpy(out, name, outSize - 1);
    } else {
        snprintf(out, outSize, "/%s", name);
    }
    out[outSize - 1] = '\0';
}

bool parseSessionIndexFromName(const char* name, uint32_t& indexOut) {
    const char* base = strrchr(name, '/');
    base = (base != nullptr) ? base + 1 : name;

    const char* prefixBase = strrchr(config::kLogFilePrefix, '/');
    prefixBase = (prefixBase != nullptr) ? prefixBase + 1 : config::kLogFilePrefix;
    size_t prefixLen = strlen(prefixBase);

    if (strncmp(base, prefixBase, prefixLen) != 0) {
        return false;
    }

    const char* numStart = base + prefixLen;
    char* endPtr = nullptr;
    long value = strtol(numStart, &endPtr, 10);
    if (endPtr == numStart || value < 0) {
        return false;
    }
    if (strcmp(endPtr, config::kLogFileSuffix) != 0) {
        return false;
    }

    indexOut = static_cast<uint32_t>(value);
    return true;
}

struct SummaryContext {
    uint32_t fileCount = 0;
    uint64_t totalBytes = 0;
};

void summaryVisitor(void* context, const char* /*path*/, uint32_t /*sessionIndex*/, size_t fileSizeBytes) {
    auto* ctx = static_cast<SummaryContext*>(context);
    ctx->fileCount++;
    ctx->totalBytes += fileSizeBytes;
}

struct OldestContext {
    const char* excludePath = nullptr;
    uint32_t bestIndex = UINT32_MAX;
    char bestPath[32] = {0};
    bool found = false;
};

void oldestVisitor(void* context, const char* path, uint32_t sessionIndex, size_t /*fileSizeBytes*/) {
    auto* ctx = static_cast<OldestContext*>(context);
    if (ctx->excludePath != nullptr && strcmp(path, ctx->excludePath) == 0) {
        return;
    }
    if (sessionIndex < ctx->bestIndex) {
        ctx->bestIndex = sessionIndex;
        strncpy(ctx->bestPath, path, sizeof(ctx->bestPath) - 1);
        ctx->found = true;
    }
}

struct FirstMatchContext {
    char path[32] = {0};
    bool found = false;
};

void firstMatchVisitor(void* context, const char* path, uint32_t /*sessionIndex*/, size_t /*fileSizeBytes*/) {
    auto* ctx = static_cast<FirstMatchContext*>(context);
    if (!ctx->found) {
        strncpy(ctx->path, path, sizeof(ctx->path) - 1);
        ctx->found = true;
    }
}

} // namespace

CsvLogger::CsvLogger(SdManager& sdManager) : sdManager_(sdManager) {}

void CsvLogger::forEachLogFile(LogFileVisitor visitor, void* context) const {
    File root = SD.open("/");
    if (!root) {
        return;
    }

    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            uint32_t sessionIndex = 0;
            if (parseSessionIndexFromName(entry.name(), sessionIndex)) {
                char path[32];
                buildPathFromName(entry.name(), path, sizeof(path));
                visitor(context, path, sessionIndex, entry.size());
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
}

void CsvLogger::writeHeader() {
    logFile_.println("timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,"
                      "voltage_v,map_kpa,iat_f,engine_load_pct,maf_gps,timing_advance_deg,"
                      "stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count");
    logFile_.flush();
}

bool CsvLogger::openNewSessionFile() {
    Preferences prefs;
    prefs.begin(config::kSessionIndexNamespace, false);
    uint32_t sessionIdx = prefs.getUInt(config::kSessionIndexKey, 0) + 1;
    prefs.putUInt(config::kSessionIndexKey, sessionIdx);
    prefs.end();

    snprintf(currentFilePath_, sizeof(currentFilePath_), "%s%0*u%s", config::kLogFilePrefix,
              static_cast<int>(config::kLogFileIndexDigits), sessionIdx, config::kLogFileSuffix);

    logFile_ = SD.open(currentFilePath_, FILE_WRITE);
    if (!logFile_) {
        Serial.printf("[Log] Failed to open %s for writing.\n", currentFilePath_);
        return false;
    }

    writeHeader();
    Serial.printf("[Log] Logging session started: %s\n", currentFilePath_);
    return true;
}

void CsvLogger::enforceCapacity() {
    if (!sdManager_.isMounted()) {
        return;
    }

    constexpr uint8_t kMaxPruneIterations = 25; // Safety bound; never loop forever.
    for (uint8_t i = 0; i < kMaxPruneIterations; ++i) {
        uint64_t totalBytes = SD.totalBytes();
        uint64_t usedBytes = SD.usedBytes();
        uint64_t freeBytes = (totalBytes > usedBytes) ? (totalBytes - usedBytes) : 0;
        if (freeBytes >= config::kSdMinFreeBytes) {
            return;
        }

        OldestContext ctx;
        ctx.excludePath = (currentFilePath_[0] != '\0') ? currentFilePath_ : nullptr;
        forEachLogFile(&oldestVisitor, &ctx);
        if (!ctx.found) {
            Serial.println("[Log] SD card low on space but no prunable logs remain.");
            return;
        }

        Serial.printf("[Log] SD card low on space; deleting oldest log %s\n", ctx.bestPath);
        SD.remove(ctx.bestPath);
    }
}

bool CsvLogger::begin() {
    if (!sdManager_.isMounted()) {
        Serial.println("[Log] SD not mounted; logging disabled.");
        loggingActive_ = false;
        return false;
    }

    enforceCapacity();

    if (!openNewSessionFile()) {
        loggingActive_ = false;
        return false;
    }

    loggingActive_ = true;
    lastRowMs_ = millis();
    lastFlushMs_ = millis();
    return true;
}

void CsvLogger::update(const TelemetrySnapshot& snapshot, uint32_t nowMs, uint32_t rowIntervalMs) {
    if (!loggingActive_) {
        return;
    }

    if (nowMs - lastRowMs_ >= rowIntervalMs) {
        lastRowMs_ = nowMs;

        char rpmF[12], speedF[12], coolantF[12], throttleF[12], fuelF[12];
        char voltageF[12], mapF[12], iatF[12], loadF[12], mafF[12];
        char timingF[12], stftF[12], ltftF[12], fuelPresF[12];
        char o2b1F[12], o2b2F[12], baroF[12], celF[4], dtcF[6];

        formatIntField(rpmF, sizeof(rpmF), snapshot.rpm);
        formatIntField(speedF, sizeof(speedF), snapshot.speedMph);
        formatIntField(coolantF, sizeof(coolantF), snapshot.coolantF);
        formatIntField(throttleF, sizeof(throttleF), snapshot.throttlePct);
        formatIntField(fuelF, sizeof(fuelF), snapshot.fuelPct);
        formatFloatField(voltageF, sizeof(voltageF), snapshot.voltageV);
        formatIntField(mapF, sizeof(mapF), snapshot.mapKpa);
        formatIntField(iatF, sizeof(iatF), snapshot.iatF);
        formatIntField(loadF, sizeof(loadF), snapshot.engineLoadPct);
        formatFloatField(mafF, sizeof(mafF), snapshot.mafGps);
        formatIntField(timingF, sizeof(timingF), snapshot.timingAdvanceDeg);
        formatIntField(stftF, sizeof(stftF), snapshot.stftPct);
        formatIntField(ltftF, sizeof(ltftF), snapshot.ltftPct);
        formatIntField(fuelPresF, sizeof(fuelPresF), snapshot.fuelPressureKpa);
        formatFloatField(o2b1F, sizeof(o2b1F), snapshot.o2B1S1V);
        formatFloatField(o2b2F, sizeof(o2b2F), snapshot.o2B2S1V);
        formatIntField(baroF, sizeof(baroF), snapshot.baroKpa);
        if (snapshot.milOn.valid) {
            snprintf(celF, sizeof(celF), "%d", (snapshot.milOn.value != 0.0F) ? 1 : 0);
        } else {
            celF[0] = '\0';
        }
        formatIntField(dtcF, sizeof(dtcF), snapshot.dtcCount);

        logFile_.printf("%lu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                         static_cast<unsigned long>(nowMs), rpmF, speedF, coolantF, throttleF, fuelF,
                         voltageF, mapF, iatF, loadF, mafF, timingF, stftF, ltftF, fuelPresF,
                         o2b1F, o2b2F, baroF, celF, dtcF);
    }

    if (nowMs - lastFlushMs_ >= config::kSdFlushIntervalMs) {
        lastFlushMs_ = nowMs;
        logFile_.flush();
        enforceCapacity();
    }
}

void CsvLogger::flush() {
    if (loggingActive_ && logFile_) {
        logFile_.flush();
    }
}

void CsvLogger::end() {
    if (logFile_) {
        logFile_.flush();
        logFile_.close();
    }
    loggingActive_ = false;
}

LogSummary CsvLogger::getLogSummary() const {
    LogSummary summary;
    if (!sdManager_.isMounted()) {
        return summary;
    }

    SummaryContext ctx;
    forEachLogFile(&summaryVisitor, &ctx);
    summary.fileCount = ctx.fileCount;
    summary.totalBytes = ctx.totalBytes;
    return summary;
}

uint32_t CsvLogger::deleteAllLogs() {
    if (!sdManager_.isMounted()) {
        return 0;
    }

    // Only resume logging afterward if it was actually active before this
    // call. Otherwise a Logs page "delete all logs" tap while SD_LOGGING_ENABLED
    // is unset (or begin() never mounted) would silently start a session.
    bool wasActive = loggingActive_;

    if (logFile_) {
        logFile_.flush();
        logFile_.close();
    }
    loggingActive_ = false;
    currentFilePath_[0] = '\0';

    uint32_t deletedCount = 0;
    constexpr uint16_t kMaxDeleteIterations = 999; // Matches the 3-digit session index ceiling.
    for (uint16_t i = 0; i < kMaxDeleteIterations; ++i) {
        FirstMatchContext ctx;
        forEachLogFile(&firstMatchVisitor, &ctx);
        if (!ctx.found) {
            break;
        }
        if (SD.remove(ctx.path)) {
            deletedCount++;
        } else {
            Serial.printf("[Log] Failed to delete %s; stopping.\n", ctx.path);
            break;
        }
    }

    Serial.printf("[Log] Deleted %lu log file(s).\n", static_cast<unsigned long>(deletedCount));

    if (wasActive && openNewSessionFile()) {
        loggingActive_ = true;
        lastRowMs_ = millis();
        lastFlushMs_ = millis();
    }

    return deletedCount;
}
