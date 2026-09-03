#include "tray/injector.h"
#include "common/diagnostics.h"
#include <windows.h>
#include <string>

#pragma comment(lib, "kernel32.lib")

namespace gpuoverlay {

bool inject_dll(const std::wstring& dllPath, unsigned long pid) {
  log_message(LogLevel::Info,
              L"Starting injection into PID " + std::to_wstring(pid));
  if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    log_last_error(LogLevel::Error, L"Hook DLL was not found");
    return false;
  }

  HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                FALSE, pid);
  if (!hProcess) {
    log_last_error(LogLevel::Error, L"OpenProcess failed");
    return false;
  }

  BOOL selfWow64 = FALSE;
  BOOL targetWow64 = FALSE;
  if (IsWow64Process(GetCurrentProcess(), &selfWow64) &&
      IsWow64Process(hProcess, &targetWow64) && selfWow64 != targetWow64) {
    log_message(LogLevel::Error,
                L"Injection architecture does not match the target process");
    CloseHandle(hProcess);
    return false;
  }

  size_t pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
  void* remoteMem = VirtualAllocEx(hProcess, nullptr, pathBytes,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remoteMem) {
    const DWORD error = GetLastError();
    CloseHandle(hProcess);
    log_last_error(LogLevel::Error, L"VirtualAllocEx failed", error);
    return false;
  }

  SIZE_T bytesWritten = 0;
  if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathBytes, &bytesWritten) ||
      bytesWritten != pathBytes) {
    const DWORD error = GetLastError();
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    log_last_error(LogLevel::Error, L"WriteProcessMemory failed", error);
    return false;
  }

  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!kernel32) {
    const DWORD error = GetLastError();
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    log_last_error(LogLevel::Error, L"kernel32.dll lookup failed", error);
    return false;
  }

  void* loadLibraryW = reinterpret_cast<void*>(GetProcAddress(kernel32, "LoadLibraryW"));
  if (!loadLibraryW) {
    const DWORD error = GetLastError();
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    log_last_error(LogLevel::Error, L"LoadLibraryW lookup failed", error);
    return false;
  }

  HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                      reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryW),
                                      remoteMem, 0, nullptr);
  if (!hThread) {
    const DWORD error = GetLastError();
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    log_last_error(LogLevel::Error, L"CreateRemoteThread failed", error);
    return false;
  }

  const DWORD waitResult = WaitForSingleObject(hThread, 10000);
  DWORD remoteResult = 0;
  const bool completed = waitResult == WAIT_OBJECT_0 &&
                         GetExitCodeThread(hThread, &remoteResult) && remoteResult != 0;
  CloseHandle(hThread);
  // The remote thread still reads this buffer if it timed out. In that rare case,
  // intentionally leave the allocation in the target instead of causing a race.
  if (waitResult == WAIT_OBJECT_0)
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
  CloseHandle(hProcess);
  if (completed) {
    log_message(LogLevel::Info,
                L"Injection completed for PID " + std::to_wstring(pid));
  } else if (waitResult == WAIT_TIMEOUT) {
    log_message(LogLevel::Error,
                L"Injection timed out for PID " + std::to_wstring(pid));
  } else {
    log_message(LogLevel::Error, L"Remote LoadLibraryW failed for PID " +
                                     std::to_wstring(pid));
  }
  return completed;
}

}  // namespace gpuoverlay
