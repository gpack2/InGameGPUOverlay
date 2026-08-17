#pragma once

#include <windows.h>
#include <string>

namespace gpuoverlay {

enum class LogLevel {
  Info,
  Warning,
  Error,
};

void initialize_diagnostics(const wchar_t* component);
void log_message(LogLevel level, const std::wstring& message);
void log_last_error(LogLevel level, const std::wstring& message,
                    DWORD error = GetLastError());
std::wstring diagnostics_directory();

}  // namespace gpuoverlay
