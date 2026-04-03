#include "tray/injector.h"
#include <windows.h>
#include <string>

#pragma comment(lib, "kernel32.lib")

namespace gpuoverlay {

bool inject_dll(const std::wstring& dllPath, unsigned long pid) {
  if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

  HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                FALSE, pid);
  if (!hProcess) return false;

  BOOL selfWow64 = FALSE;
  BOOL targetWow64 = FALSE;
  if (IsWow64Process(GetCurrentProcess(), &selfWow64) &&
      IsWow64Process(hProcess, &targetWow64) && selfWow64 != targetWow64) {
    CloseHandle(hProcess);
    return false;
  }

  size_t pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
  void* remoteMem = VirtualAllocEx(hProcess, nullptr, pathBytes,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remoteMem) {
    CloseHandle(hProcess);
    return false;
  }

  SIZE_T bytesWritten = 0;
  if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathBytes, &bytesWritten) ||
      bytesWritten != pathBytes) {
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false;
  }

  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!kernel32) {
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false;
  }

  void* loadLibraryW = reinterpret_cast<void*>(GetProcAddress(kernel32, "LoadLibraryW"));
  if (!loadLibraryW) {
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false;
  }

  HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                      reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryW),
                                      remoteMem, 0, nullptr);
  if (!hThread) {
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
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
  return completed;
}

}  // namespace gpuoverlay
