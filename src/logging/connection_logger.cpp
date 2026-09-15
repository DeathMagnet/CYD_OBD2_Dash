#include "logging/connection_logger.h"
#include <Arduino.h>
#include <SD.h>
#include <cstdarg>
#include "app_config.h"
#include "storage/sd_manager.h"

namespace connection_log {

void beginSession(SdManager& sdManager) {
    if (!sdManager.isMounted()) {
        return;
    }
    sdManager.lock();
    if (SD.exists(config::kConnectionLogFilePath)) {
        SD.remove(config::kConnectionLogFilePath);
    }
    sdManager.unlock();
}

void write(SdManager& sdManager, const char* message) {
    if (!sdManager.isMounted()) {
        return;
    }

    sdManager.lock();

    if (!SD.exists(config::kLogDirPath)) {
        SD.mkdir(config::kLogDirPath);
    }

    File logFile = SD.open(config::kConnectionLogFilePath, FILE_APPEND);
    if (!logFile) {
        sdManager.unlock();
        return;
    }

    uint32_t nowMs = millis();
    logFile.printf("[%u] %s\n", nowMs, message);
    logFile.flush();
    logFile.close();

    sdManager.unlock();
}

void writef(SdManager& sdManager, const char* fmt, ...) {
    if (!sdManager.isMounted()) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);

    if (len > 0) {
        buffer[len] = '\0';
        write(sdManager, buffer);
    }
}

}
