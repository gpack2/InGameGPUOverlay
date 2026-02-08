#pragma once

#include <string>
#include <vector>

namespace gpuoverlay {

struct ProcessInfo {
  unsigned long pid = 0;
  std::wstring name;
  std::wstring path;
};

// List processes that might be games (has a window, 64-bit preferred for Steam).
std::vector<ProcessInfo> get_process_list();

}  // namespace gpuoverlay
