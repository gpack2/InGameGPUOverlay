#pragma once

#include <windows.h>
#include <functional>
#include <string>

namespace gpuoverlay {

inline constexpr UINT kTrayIconMessage = WM_APP + 1;
inline constexpr UINT kTrayInjectCommand = 1001;
inline constexpr UINT kTrayExitCommand = 1002;

// Create tray icon. callback(id) when menu item selected: 1 = Inject, 2 = Exit.
bool tray_init(HWND hwnd, const wchar_t* tip);
void tray_shutdown();
void tray_show_menu(HWND hwnd, int x, int y);

// Show process picker dialog; returns PID or 0 if cancelled.
unsigned long show_inject_dialog(HWND parent, const std::wstring& dllPath);

}  // namespace gpuoverlay
