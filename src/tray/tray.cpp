#include "tray/tray.h"
#include "tray/process_list.h"
#include "tray/injector.h"
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

namespace gpuoverlay {

#define WM_TRAYICON (WM_USER + 1)
#define IDC_LIST      2001
#define IDC_INJECT     2002
#define IDC_CANCEL     2003

static NOTIFYICONDATAW s_nid = {};
static bool s_trayAdded = false;

bool tray_init(HWND hwnd, const wchar_t* tip) {
  s_nid.cbSize = sizeof(s_nid);
  s_nid.hWnd = hwnd;
  s_nid.uID = 1;
  s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  s_nid.uCallbackMessage = WM_TRAYICON;
  s_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  if (tip) wcsncpy_s(s_nid.szTip, tip, _TRUNCATE);

  if (!Shell_NotifyIconW(NIM_ADD, &s_nid)) return false;
  s_trayAdded = true;
  return true;
}

void tray_shutdown() {
  if (s_trayAdded) {
    Shell_NotifyIconW(NIM_DELETE, &s_nid);
    s_trayAdded = false;
  }
}

void tray_show_menu(HWND hwnd, int x, int y) {
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, ID_TRAY_INJECT, L"Inject into process...");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

  SetForegroundWindow(hwnd);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hwnd, nullptr);
  DestroyMenu(menu);
  if (cmd) PostMessageW(hwnd, WM_COMMAND, cmd, 0);
}

static std::wstring* s_injectDllPath = nullptr;
static unsigned long s_injectResult = 0;

static LRESULT CALLBACK InjectWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      s_injectDllPath = reinterpret_cast<std::wstring*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
      HWND list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                  WS_CHILD | WS_VISIBLE | LBS_STANDARD,
                                  10, 10, 360, 220, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST)),
                                  GetModuleHandleW(nullptr), nullptr);
      CreateWindowW(L"BUTTON", L"Inject", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                   10, 240, 100, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INJECT)),
                   GetModuleHandleW(nullptr), nullptr);
      CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                   120, 240, 100, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CANCEL)),
                   GetModuleHandleW(nullptr), nullptr);

      auto procs = get_process_list();
      for (const auto& p : procs) {
        std::wstring display = p.name + L" (PID: " + std::to_wstring(p.pid) + L")";
        int idx = static_cast<int>(SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(display.c_str())));
        SendMessageW(list, LB_SETITEMDATA, idx, static_cast<LPARAM>(p.pid));
      }
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == IDC_INJECT) {
        HWND list = GetDlgItem(hwnd, IDC_LIST);
        int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (sel >= 0 && s_injectDllPath) {
          unsigned long pid = static_cast<unsigned long>(SendMessageW(list, LB_GETITEMDATA, sel, 0));
          if (inject_dll(*s_injectDllPath, pid)) {
            MessageBoxW(hwnd, L"Injection started. The overlay will appear in the selected process when it uses DirectX 11.", L"GPU Overlay", MB_OK | MB_ICONINFORMATION);
            s_injectResult = pid;
          } else {
            MessageBoxW(hwnd, L"Injection failed. Run as Administrator or choose a different process.", L"GPU Overlay", MB_OK | MB_ICONWARNING);
          }
        }
        DestroyWindow(hwnd);
        return 0;
      }
      if (LOWORD(wParam) == IDC_CANCEL) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

unsigned long show_inject_dialog(HWND parent, const std::wstring& dllPath) {
  s_injectResult = 0;
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = InjectWndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"GPUOverlayInjectDialog";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  HWND hwnd = CreateWindowExW(0, L"GPUOverlayInjectDialog", L"Inject GPU Overlay",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT, 400, 320,
                              parent, nullptr, wc.hInstance, const_cast<std::wstring*>(&dllPath));
  if (!hwnd) return 0;
  ShowWindow(hwnd, SW_SHOW);

  MSG msg;
  while (IsWindow(hwnd) && GetMessage(&msg, nullptr, 0, 0)) {
    DispatchMessage(&msg);
  }
  return s_injectResult;
}

}  // namespace gpuoverlay
