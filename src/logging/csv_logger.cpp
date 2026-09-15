#include "logging/csv_logger.h"
#include "system/units.h"
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

// name may come back as "/logs/obd_log_007.csv", "obd_log_007.csv", or other
// variations depending on the ESP32 core version; normalize to the basename
// joined with the log directory.
void buildPathFromName(const char* name, char* out, size_t outSize) {
    const char* base = strrchr(name, '/');
    base = (base != nullptr) ? base + 1 : name;
    snprintf(out, outSize, "%s/%s", config::kLogDirPath, base);
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

struct DeleteContext {
    uint32_t deletedCount = 0;
};

void deleteVisitor(void* context, const char* path, uint32_t /*sessionIndex*/, size_t /*fileSizeBytes*/) {
    auto* ctx = static_cast<DeleteContext*>(context);
    if (SD.remove(path)) {
        ctx->deletedCount++;
    } else {
        Serial.printf("[Log] Failed to delete %s.\n", path);
    }
}

} // namespace

CsvLogger::CsvLogger(SdManager& sdManager) : sdManager_(sdManager) {}

void CsvLogger::forEachLogFile(LogFileVisitor visitor, void* context) const {
    File root = SD.open(config::kLogDirPath);
    if (!root) {
        return;
    }

    File entry = root.openNextFile();
    while (entry) {
        bool matched = false;
        uint32_t sessionIndex = 0;
        char path[32];
        size_t fileSize = 0;
        if (!entry.isDirectory()) {
            matched = parseSessionIndexFromName(entry.name(), sessionIndex);
            if (matched) {
                buildPathFromName(entry.name(), path, sizeof(path));
                fileSize = entry.size();
            }
        }
        // Close the entry before invoking the visitor: a visitor that
        // deletes the file (e.g. deleteAllLogs()) can fail or behave
        // unpredictably on some SD/FAT stacks if the file handle is still
        // open when SD.remove() is called.
        entry.close();
        if (matched) {
            visitor(context, path, sessionIndex, fileSize);
        }
        entry = root.openNextFile();
    }
    root.close();
}

void CsvLogger::writeHeader() {
    if (useMetricLogs_) {
        logFile_.println("timestamp_ms,rpm,speed_kph,coolant_c,throttle_pct,fuel_pct,"
                          "voltage_v,map_kpa,iat_c,engine_load_pct,maf_gps,timing_advance_deg,"
                          "stft_pct,ltft_pct,fuel_pressure_kpa,o2_b1s1_v,o2_b2s1_v,baro_kpa,cel_on,dtc_count");
    } else {
        logFile_.println("timestamp_ms,rpm,speed_mph,coolant_f,throttle_pct,fuel_pct,"
                          "voltage_v,map_psi,iat_f,engine_load_pct,maf_gps,timing_advance_deg,"
                          "stft_pct,ltft_pct,fuel_pressure_psi,o2_b1s1_v,o2_b2s1_v,baro_psi,cel_on,dtc_count");
    }
    logFile_.flush();
}

bool CsvLogger::openNewSessionFile() {
    sdManager_.lock();

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
        sdManager_.unlock();
        return false;
    }

    writeHeader();
    Serial.printf("[Log] Logging session started: %s\n", currentFilePath_);

    sdManager_.unlock();
    return true;
}

void CsvLogger::enforceCapacity() {
    if (!sdManager_.isMounted()) {
        return;
    }

    sdManager_.lock();

    constexpr uint8_t kMaxPruneIterations = 25; // Safety bound; never loop forever.
    for (uint8_t i = 0; i < kMaxPruneIterations; ++i) {
        uint64_t totalBytes = SD.totalBytes();
        uint64_t usedBytes = SD.usedBytes();
        uint64_t freeBytes = (totalBytes > usedBytes) ? (totalBytes - usedBytes) : 0;
        if (freeBytes >= config::kSdMinFreeBytes) {
            sdManager_.unlock();
            return;
        }

        OldestContext ctx;
        ctx.excludePath = (currentFilePath_[0] != '\0') ? currentFilePath_ : nullptr;
        forEachLogFile(&oldestVisitor, &ctx);
        if (!ctx.found) {
            Serial.println("[Log] SD card low on space but no prunable logs remain.");
            sdManager_.unlock();
            return;
        }

        Serial.printf("[Log] SD card low on space; deleting oldest log %s\n", ctx.bestPath);
        SD.remove(ctx.bestPath);
    }

    sdManager_.unlock();
}

bool CsvLogger::begin() {
    if (!sdManager_.isMounted()) {
        Serial.println("[Log] SD not mounted; logging disabled.");
        loggingActive_ = false;
        return false;
    }

    if (!SD.exists(config::kLogDirPath)) {
        SD.mkdir(config::kLogDirPath);
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

        // Speed: convert if metric
        TelemetryValue speedValue = snapshot.speedMph;
        if (useMetricLogs_ && speedValue.valid) {
            speedValue.value = units::kphFromMph(speedValue.value);
        }
        formatIntField(speedF, sizeof(speedF), speedValue);

        // Coolant: convert if metric
        TelemetryValue coolantValue = snapshot.coolantF;
        if (useMetricLogs_ && coolantValue.valid) {
            coolantValue.value = units::celsiusFromFahrenheit(coolantValue.value);
        }
        formatIntField(coolantF, sizeof(coolantF), coolantValue);

        formatIntField(throttleF, sizeof(throttleF), snapshot.throttlePct);
        formatIntField(fuelF, sizeof(fuelF), snapshot.fuelPct);
        formatFloatField(voltageF, sizeof(voltageF), snapshot.voltageV);

        // MAP: convert if not metric
        TelemetryValue mapValue = snapshot.mapKpa;
        if (!useMetricLogs_ && mapValue.valid) {
            mapValue.value = units::psiFromKpa(mapValue.value);
        }
        formatIntField(mapF, sizeof(mapF), mapValue);

        // IAT: convert if metric
        TelemetryValue iatValue = snapshot.iatF;
        if (useMetricLogs_ && iatValue.valid) {
            iatValue.value = units::celsiusFromFahrenheit(iatValue.value);
        }
        formatIntField(iatF, sizeof(iatF), iatValue);

        formatIntField(loadF, sizeof(loadF), snapshot.engineLoadPct);
        formatFloatField(mafF, sizeof(mafF), snapshot.mafGps);
        formatIntField(timingF, sizeof(timingF), snapshot.timingAdvanceDeg);
        formatIntField(stftF, sizeof(stftF), snapshot.stftPct);
        formatIntField(ltftF, sizeof(ltftF), snapshot.ltftPct);

        // Fuel Pressure: convert if not metric
        TelemetryValue fuelPresValue = snapshot.fuelPressureKpa;
        if (!useMetricLogs_ && fuelPresValue.valid) {
            fuelPresValue.value = units::psiFromKpa(fuelPresValue.value);
        }
        formatIntField(fuelPresF, sizeof(fuelPresF), fuelPresValue);

        formatFloatField(o2b1F, sizeof(o2b1F), snapshot.o2B1S1V);
        formatFloatField(o2b2F, sizeof(o2b2F), snapshot.o2B2S1V);

        // Baro: convert if not metric
        TelemetryValue baroValue = snapshot.baroKpa;
        if (!useMetricLogs_ && baroValue.valid) {
            baroValue.value = units::psiFromKpa(baroValue.value);
        }
        formatIntField(baroF, sizeof(baroF), baroValue);

        if (snapshot.milOn.valid) {
            snprintf(celF, sizeof(celF), "%d", (snapshot.milOn.value != 0.0F) ? 1 : 0);
        } else {
            celF[0] = '\0';
        }
        formatIntField(dtcF, sizeof(dtcF), snapshot.dtcCount);

        sdManager_.lock();
        logFile_.printf("%lu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                         static_cast<unsigned long>(nowMs), rpmF, speedF, coolantF, throttleF, fuelF,
                         voltageF, mapF, iatF, loadF, mafF, timingF, stftF, ltftF, fuelPresF,
                         o2b1F, o2b2F, baroF, celF, dtcF);
        sdManager_.unlock();
    }

    if (nowMs - lastFlushMs_ >= config::kSdFlushIntervalMs) {
        lastFlushMs_ = nowMs;
        sdManager_.lock();
        logFile_.flush();
        sdManager_.unlock();
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

    // A single directory pass that deletes each matching file as it's found,
    // rather than rescanning the whole SD directory from scratch per file
    // (that O(n^2) pattern could stall loop()/touch input for a long time
    // once a lot of session logs had accumulated).
    DeleteContext ctx;
    forEachLogFile(&deleteVisitor, &ctx);
    uint32_t deletedCount = ctx.deletedCount;

    Serial.printf("[Log] Deleted %lu log file(s).\n", static_cast<unsigned long>(deletedCount));

    if (wasActive && openNewSessionFile()) {
        loggingActive_ = true;
        lastRowMs_ = millis();
        lastFlushMs_ = millis();
    }

    return deletedCount;
}
