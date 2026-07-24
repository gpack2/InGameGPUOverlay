#include "tray/settings_dialog.h"
#include "common/overlay_config.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <iterator>

namespace gpuoverlay {

namespace {

constexpr wchar_t kSettingsClass[] = L"GPUOverlaySettingsDialog";
constexpr int kPosition = 3001;
constexpr int kScale = 3002;
constexpr int kTextColor = 3003;
constexpr int kBackgroundColor = 3004;
constexpr int kGpuUsage = 3005;
constexpr int kVram = 3006;
constexpr int kFps = 3007;
constexpr int kClock = 3008;
constexpr int kTemperature = 3009;
constexpr int kToggleHotkey = 3010;
constexpr int kPositionHotkey = 3011;
constexpr int kSave = 3012;
constexpr int kCancel = 3013;

struct SettingsState {
  std::wstring path;
  OverlayConfig config;
  bool saved = false;
};

HWND add_control(HWND parent, const wchar_t* className, const wchar_t* text,
                 DWORD style, int x, int y, int width, int height, int id) {
  return CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
                         x, y, width, height, parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr), nullptr);
}

void set_text(HWND window, int id, const std::wstring& value) {
  SetWindowTextW(GetDlgItem(window, id), value.c_str());
}

std::wstring get_text(HWND window, int id) {
  wchar_t buffer[128] = {};
  GetWindowTextW(GetDlgItem(window, id), buffer,
                 static_cast<int>(std::size(buffer)));
  return buffer;
}

void set_checked(HWND window, int id, bool checked) {
  SendDlgItemMessageW(window, id, BM_SETCHECK,
                      checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool is_checked(HWND window, int id) {
  return SendDlgItemMessageW(window, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void add_label(HWND window, const wchar_t* text, int x, int y, int width) {
  add_control(window, L"STATIC", text, 0, x, y, width, 22, 0);
}

void initialize_controls(HWND window, SettingsState& state) {
  add_label(window, L"Position", 20, 22, 130);
  HWND position = add_control(window, L"COMBOBOX", L"",
                              CBS_DROPDOWNLIST | WS_VSCROLL, 165, 18, 270, 150,
                              kPosition);
  SendMessageW(position, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Top left"));
  SendMessageW(position, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Top right"));
  SendMessageW(position, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bottom left"));
  SendMessageW(position, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bottom right"));
  SendMessageW(position, CB_SETCURSEL, static_cast<WPARAM>(state.config.position), 0);

  add_label(window, L"Scale (0.5-3.0)", 20, 62, 130);
  add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 165, 58, 270, 24,
              kScale);
  wchar_t scale[32] = {};
  swprintf_s(scale, L"%.2f", state.config.scale);
  set_text(window, kScale, scale);

  add_label(window, L"Text color", 20, 102, 130);
  add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 165, 98, 270, 24,
              kTextColor);
  set_text(window, kTextColor, color_to_string(state.config.textColor));

  add_label(window, L"Background color", 20, 142, 130);
  add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 165, 138, 270,
              24, kBackgroundColor);
  set_text(window, kBackgroundColor,
           color_to_string(state.config.backgroundColor));

  add_label(window, L"Visible metrics", 20, 182, 130);
  add_control(window, L"BUTTON", L"GPU usage", BS_AUTOCHECKBOX, 165, 178, 130, 24,
              kGpuUsage);
  add_control(window, L"BUTTON", L"VRAM", BS_AUTOCHECKBOX, 305, 178, 130, 24,
              kVram);
  add_control(window, L"BUTTON", L"FPS", BS_AUTOCHECKBOX, 165, 208, 130, 24,
              kFps);
  add_control(window, L"BUTTON", L"GPU clock", BS_AUTOCHECKBOX, 305, 208, 130,
              24, kClock);
  add_control(window, L"BUTTON", L"Temperature", BS_AUTOCHECKBOX, 165, 238, 130,
              24, kTemperature);
  set_checked(window, kGpuUsage, state.config.showGpuUsage);
  set_checked(window, kVram, state.config.showVram);
  set_checked(window, kFps, state.config.showFps);
  set_checked(window, kClock, state.config.showClock);
  set_checked(window, kTemperature, state.config.showTemperature);

  add_label(window, L"Toggle hotkey", 20, 288, 130);
  add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 165, 284, 270,
              24, kToggleHotkey);
  set_text(window, kToggleHotkey, hotkey_to_string(state.config.toggleHotkey));

  add_label(window, L"Cycle position", 20, 328, 130);
  add_control(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 165, 324, 270,
              24, kPositionHotkey);
  set_text(window, kPositionHotkey,
           hotkey_to_string(state.config.cyclePositionHotkey));

  add_label(window, L"Hotkeys support Ctrl, Alt, Shift, A-Z, 0-9, F1-F24, and navigation keys.",
            20, 365, 415);

  add_control(window, L"BUTTON", L"Save", BS_DEFPUSHBUTTON, 235, 410, 95, 30,
              kSave);
  add_control(window, L"BUTTON", L"Cancel", BS_PUSHBUTTON, 340, 410, 95, 30,
              kCancel);
}

bool save_controls(HWND window, SettingsState& state) {
  OverlayConfig config = state.config;
  const LRESULT position = SendDlgItemMessageW(window, kPosition, CB_GETCURSEL, 0, 0);
  if (position >= 0 && position <= 3)
    config.position = static_cast<OverlayPosition>(position);

  const std::wstring scaleText = get_text(window, kScale);
  wchar_t* scaleEnd = nullptr;
  const float scale = std::wcstof(scaleText.c_str(), &scaleEnd);
  if (!scaleEnd || *scaleEnd != L'\0' || !std::isfinite(scale) ||
      scale < 0.5f || scale > 3.0f) {
    MessageBoxW(window, L"Scale must be a number from 0.5 through 3.0.",
                L"GPU Overlay", MB_OK | MB_ICONWARNING);
    return false;
  }
  config.scale = scale;

  if (!parse_color(get_text(window, kTextColor), config.textColor) ||
      !parse_color(get_text(window, kBackgroundColor), config.backgroundColor)) {
    MessageBoxW(window, L"Colors must use #RRGGBB format.", L"GPU Overlay",
                MB_OK | MB_ICONWARNING);
    return false;
  }

  config.showGpuUsage = is_checked(window, kGpuUsage);
  config.showVram = is_checked(window, kVram);
  config.showFps = is_checked(window, kFps);
  config.showClock = is_checked(window, kClock);
  config.showTemperature = is_checked(window, kTemperature);

  if (!parse_hotkey(get_text(window, kToggleHotkey), config.toggleHotkey) ||
      !parse_hotkey(get_text(window, kPositionHotkey),
                    config.cyclePositionHotkey)) {
    MessageBoxW(window, L"Enter hotkeys like F11 or Ctrl+Shift+F10.",
                L"GPU Overlay", MB_OK | MB_ICONWARNING);
    return false;
  }

  if (!save_overlay_config(state.path, config)) {
    MessageBoxW(window, L"The settings file could not be written.", L"GPU Overlay",
                MB_OK | MB_ICONERROR);
    return false;
  }
  state.config = config;
  state.saved = true;
  return true;
}

LRESULT CALLBACK settings_proc(HWND window, UINT message, WPARAM wParam,
                               LPARAM lParam) {
  auto* state = reinterpret_cast<SettingsState*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  switch (message) {
    case WM_CREATE: {
      const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
      state = static_cast<SettingsState*>(create->lpCreateParams);
      SetWindowLongPtrW(window, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(state));
      initialize_controls(window, *state);
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == kSave && state && save_controls(window, *state)) {
        DestroyWindow(window);
        return 0;
      }
      if (LOWORD(wParam) == kCancel) {
        DestroyWindow(window);
        return 0;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(window);
      return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

bool show_settings_dialog(HWND parent, const std::wstring& configPath) {
  SettingsState state{configPath, load_overlay_config(configPath), false};

  WNDCLASSEXW windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = settings_proc;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.lpszClassName = kSettingsClass;
  windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (!RegisterClassExW(&windowClass) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return false;

  HWND window = CreateWindowExW(
      WS_EX_DLGMODALFRAME, kSettingsClass, L"GPU Overlay Settings",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
      475, 500, parent, nullptr, windowClass.hInstance, &state);
  if (!window) return false;

  EnableWindow(parent, FALSE);
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);

  MSG message = {};
  BOOL result = TRUE;
  while (IsWindow(window) && (result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  EnableWindow(parent, TRUE);
  SetForegroundWindow(parent);
  if (result == 0) PostQuitMessage(static_cast<int>(message.wParam));
  return state.saved;
}

}  // namespace gpuoverlay
