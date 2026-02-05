#include "tray/injector.h"
#include <windows.h>
#include <string>

#pragma comment(lib, "kernel32.lib")

namespace gpuoverlay {

bool inject_dll(const std::wstring& dllPath, unsigned long pid) {
  HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                FALSE, pid);
  if (!hProcess) return false;

  size_t pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
  void* remoteMem = VirtualAllocEx(hProcess, nullptr, pathBytes,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remoteMem) {
    CloseHandle(hProcess);
    return false;
  }

  if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathBytes, nullptr)) {
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

  WaitForSingleObject(hThread, 10000);
  CloseHandle(hThread);
  VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
  CloseHandle(hProcess);
  return true;
}

}  // namespace gpuoverlay
