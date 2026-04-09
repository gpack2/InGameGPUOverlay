#include "tray/tray.h"
#include <windows.h>
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
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kWindowClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&wc)) return 1;

  HWND hwnd = CreateWindowExW(0, kWindowClass, L"GPU Overlay",
                              WS_OVERLAPPED, 0, 0, 0, 0,
                              nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) return 1;

  if (!gpuoverlay::tray_init(hwnd, L"GPU Overlay - Right-click to inject")) {
    DestroyWindow(hwnd);
    return 1;
  }

  ShowWindow(hwnd, SW_HIDE);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return 0;
}
