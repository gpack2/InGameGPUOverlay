#include "tray/tray.h"
#include "tray/settings_dialog.h"
#include "common/overlay_config.h"
#include "common/diagnostics.h"
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

namespace {

const wchar_t* kWindowClass = L"GPUOverlayTrayWindow";
std::wstring g_dllPath;

std::wstring get_dll_path() {
  wchar_t path[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return {};
  std::wstring exePath(path);
  size_t lastSlash = exePath.find_last_of(L"\\/");
  if (lastSlash == std::wstring::npos) return {};
  return exePath.substr(0, lastSlash + 1) + L"GPUOverlayHook.dll";
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case gpuoverlay::kTrayIconMessage:
      if (lParam == WM_RBUTTONUP) {
        POINT pt;
        GetCursorPos(&pt);
        gpuoverlay::tray_show_menu(hwnd, pt.x, pt.y);
      }
      return 0;
    case WM_COMMAND:
      if (LOWORD(wParam) == gpuoverlay::kTrayInjectCommand) {
        g_dllPath = get_dll_path();
        if (g_dllPath.empty()) {
          MessageBoxW(hwnd, L"GPUOverlayHook.dll not found next to executable.", L"GPU Overlay", MB_OK | MB_ICONERROR);
        } else {
          gpuoverlay::show_inject_dialog(hwnd, g_dllPath);
        }
        return 0;
      }
      if (LOWORD(wParam) == gpuoverlay::kTrayExitCommand) {
        PostQuitMessage(0);
        return 0;
      }
      if (LOWORD(wParam) == gpuoverlay::kTraySettingsCommand) {
        gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                                L"Opening settings");
        if (gpuoverlay::show_settings_dialog(
                hwnd, gpuoverlay::default_config_path()))
          gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                                  L"Settings saved");
        return 0;
      }
      if (LOWORD(wParam) == gpuoverlay::kTrayLogsCommand) {
        const std::wstring directory = gpuoverlay::diagnostics_directory();
        const HINSTANCE result = ShellExecuteW(hwnd, L"open", directory.c_str(),
                                               nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) {
          gpuoverlay::log_message(gpuoverlay::LogLevel::Error,
                                  L"Could not open the log folder");
          MessageBoxW(hwnd, L"The log folder could not be opened.",
                      L"GPU Overlay", MB_OK | MB_ICONERROR);
        }
        return 0;
      }
      break;
    case WM_DESTROY:
      gpuoverlay::tray_shutdown();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  gpuoverlay::initialize_diagnostics(L"tray");
  gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                          L"GPU Overlay tray app starting");
  const std::wstring configPath = gpuoverlay::default_config_path();
  gpuoverlay::ensure_overlay_config(configPath);
  gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                          L"Configuration: " + configPath);
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kWindowClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wc)) {
    gpuoverlay::log_last_error(gpuoverlay::LogLevel::Error,
                               L"Tray window registration failed");
    return 1;
  }

  HWND hwnd = CreateWindowExW(0, kWindowClass, L"GPU Overlay",
                              WS_OVERLAPPED, 0, 0, 0, 0,
                              nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) {
    gpuoverlay::log_last_error(gpuoverlay::LogLevel::Error,
                               L"Tray window creation failed");
    return 1;
  }

  if (!gpuoverlay::tray_init(hwnd, L"GPU Overlay - Right-click to inject")) {
    gpuoverlay::log_last_error(gpuoverlay::LogLevel::Error,
                               L"System tray icon creation failed");
    DestroyWindow(hwnd);
    return 1;
  }

  ShowWindow(hwnd, SW_HIDE);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                          L"GPU Overlay tray app stopped");
  return 0;
}
