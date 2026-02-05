#include "tray/process_list.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

#pragma comment(lib, "kernel32.lib")

namespace gpuoverlay {

std::vector<ProcessInfo> get_process_list() {
  std::vector<ProcessInfo> list;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return list;

  PROCESSENTRY32W pe = {};
  pe.dwSize = sizeof(pe);
  if (!Process32FirstW(snap, &pe)) {
    CloseHandle(snap);
    return list;
  }

  do {
    if (pe.th32ProcessID == 0) continue;
    ProcessInfo info;
    info.pid = pe.th32ProcessID;
    info.name = pe.szExeFile;

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
    if (h) {
      wchar_t path[MAX_PATH] = {};
      DWORD size = MAX_PATH;
      if (QueryFullProcessImageNameW(h, 0, path, &size))
        info.path = path;
      CloseHandle(h);
    }
    list.push_back(info);
  } while (Process32NextW(snap, &pe));

  CloseHandle(snap);
  return list;
}

}  // namespace gpuoverlay
