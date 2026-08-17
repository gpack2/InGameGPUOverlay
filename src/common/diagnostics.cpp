#include "common/diagnostics.h"
#include <algorithm>
#include <cwctype>
#include <iterator>
#include <mutex>
#include <sstream>

namespace gpuoverlay {

namespace {

constexpr ULONGLONG kMaxLogBytes = 1024 * 1024;
constexpr int kLogGenerations = 3;
std::mutex g_logMutex;
std::wstring g_component = L"app";

std::wstring join_path(const std::wstring& left, const std::wstring& right) {
  if (left.empty()) return right;
  if (left.back() == L'\\' || left.back() == L'/') return left + right;
  return left + L"\\" + right;
}

void ensure_directory(const std::wstring& path) {
  if (path.empty() || GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
    return;
  const size_t slash = path.find_last_of(L"\\/");
  if (slash != std::wstring::npos) ensure_directory(path.substr(0, slash));
  CreateDirectoryW(path.c_str(), nullptr);
}

std::wstring component_path(int generation = 0) {
  std::wstring name = g_component + L".log";
  if (generation > 0) name += L"." + std::to_wstring(generation);
  return join_path(diagnostics_directory(), name);
}

void rotate_if_needed() {
  WIN32_FILE_ATTRIBUTE_DATA data = {};
  const std::wstring current = component_path();
  if (!GetFileAttributesExW(current.c_str(), GetFileExInfoStandard, &data)) return;
  ULARGE_INTEGER size = {};
  size.HighPart = data.nFileSizeHigh;
  size.LowPart = data.nFileSizeLow;
  if (size.QuadPart < kMaxLogBytes) return;

  DeleteFileW(component_path(kLogGenerations).c_str());
  for (int generation = kLogGenerations - 1; generation >= 1; --generation) {
    MoveFileExW(component_path(generation).c_str(),
                component_path(generation + 1).c_str(),
                MOVEFILE_REPLACE_EXISTING);
  }
  MoveFileExW(current.c_str(), component_path(1).c_str(),
              MOVEFILE_REPLACE_EXISTING);
}

const wchar_t* level_name(LogLevel level) {
  switch (level) {
    case LogLevel::Warning: return L"WARN";
    case LogLevel::Error: return L"ERROR";
    default: return L"INFO";
  }
}

std::string utf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr,
                                         0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}

}  // namespace

std::wstring diagnostics_directory() {
  wchar_t localAppData[32768] = {};
  DWORD length = GetEnvironmentVariableW(
      L"LOCALAPPDATA", localAppData,
      static_cast<DWORD>(std::size(localAppData)));
  std::wstring base;
  if (length > 0 && length < static_cast<DWORD>(std::size(localAppData))) {
    base.assign(localAppData, length);
  } else {
    wchar_t temp[MAX_PATH] = {};
    const DWORD tempLength = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    if (tempLength > 0 && tempLength < static_cast<DWORD>(std::size(temp)))
      base.assign(temp, tempLength);
  }
  const std::wstring directory = join_path(join_path(base, L"GPUOverlay"), L"Logs");
  ensure_directory(directory);
  return directory;
}

void initialize_diagnostics(const wchar_t* component) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_component = component && *component ? component : L"app";
  std::replace_if(g_component.begin(), g_component.end(), [](wchar_t ch) {
    return !std::iswalnum(ch) && ch != L'-' && ch != L'_';
  }, L'_');
  rotate_if_needed();
}

void log_message(LogLevel level, const std::wstring& message) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  rotate_if_needed();

  SYSTEMTIME time = {};
  GetLocalTime(&time);
  std::wostringstream line;
  line << time.wYear << L'-';
  line.width(2); line.fill(L'0'); line << time.wMonth << L'-';
  line.width(2); line << time.wDay << L' ';
  line.width(2); line << time.wHour << L':';
  line.width(2); line << time.wMinute << L':';
  line.width(2); line << time.wSecond << L'.';
  line.width(3); line << time.wMilliseconds;
  line << L" [" << level_name(level) << L"] [PID " << GetCurrentProcessId()
       << L"] " << message << L"\r\n";
  const std::string bytes = utf8(line.str());
  if (bytes.empty()) return;

  const std::wstring path = component_path();
  HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written,
            nullptr);
  CloseHandle(file);
}

void log_last_error(LogLevel level, const std::wstring& message, DWORD error) {
  log_message(level, message + L" (Win32 error " + std::to_wstring(error) + L")");
}

}  // namespace gpuoverlay
