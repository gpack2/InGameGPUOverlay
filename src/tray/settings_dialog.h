#pragma once

#include <windows.h>
#include <string>

namespace gpuoverlay {

bool show_settings_dialog(HWND parent, const std::wstring& configPath);

}  // namespace gpuoverlay
