#pragma once

class SdManager;

namespace connection_log {

void beginSession(SdManager& sdManager);
void write(SdManager& sdManager, const char* message);
void writef(SdManager& sdManager, const char* fmt, ...);

}
