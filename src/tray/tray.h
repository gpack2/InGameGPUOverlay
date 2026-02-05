#pragma once

#include <windows.h>
#include <functional>
#include <string>

#define ID_TRAY_INJECT 1001
#define ID_TRAY_EXIT    1002

namespace gpuoverlay {

// Create tray icon. callback(id) when menu item selected: 1 = Inject, 2 = Exit.
bool tray_init(HWND hwnd, const wchar_t* tip);
void tray_shutdown();
void tray_show_menu(HWND hwnd, int x, int y);

// Show process picker dialog; returns PID or 0 if cancelled.
unsigned long show_inject_dialog(HWND parent, const std::wstring& dllPath);

}  // namespace gpuoverlay
