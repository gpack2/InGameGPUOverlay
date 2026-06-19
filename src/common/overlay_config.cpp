#include "common/overlay_config.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <sstream>
#include <vector>

namespace gpuoverlay {

namespace {

std::wstring trim(std::wstring value) {
  const auto notSpace = [](wchar_t ch) { return !std::iswspace(ch); };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), notSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
              value.end());
  return value;
}

std::wstring upper(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towupper(ch)); });
  return value;
}

std::wstring read_value(const std::wstring& path, const wchar_t* section,
                        const wchar_t* key, const wchar_t* fallback) {
  wchar_t buffer[256] = {};
  GetPrivateProfileStringW(section, key, fallback, buffer,
                           static_cast<DWORD>(std::size(buffer)), path.c_str());
  return buffer;
}

bool read_bool(const std::wstring& path, const wchar_t* section,
               const wchar_t* key, bool fallback) {
  const std::wstring value = upper(trim(read_value(
      path, section, key, fallback ? L"true" : L"false")));
  return value == L"1" || value == L"TRUE" || value == L"YES" ||
         value == L"ON";
}

bool write_value(const std::wstring& path, const wchar_t* section,
                 const wchar_t* key, const std::wstring& value) {
  return WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) !=
         FALSE;
}

bool write_bool(const std::wstring& path, const wchar_t* section,
                const wchar_t* key, bool value) {
  return write_value(path, section, key, value ? L"true" : L"false");
}

UINT key_from_name(const std::wstring& name) {
  if (name.size() == 1 && name[0] >= L'A' && name[0] <= L'Z') return name[0];
  if (name.size() == 1 && name[0] >= L'0' && name[0] <= L'9') return name[0];
  if (name.size() >= 2 && name[0] == L'F') {
    wchar_t* end = nullptr;
    const long number = std::wcstol(name.c_str() + 1, &end, 10);
    if (end && *end == L'\0' && number >= 1 && number <= 24)
      return static_cast<UINT>(VK_F1 + number - 1);
  }
  if (name == L"HOME") return VK_HOME;
  if (name == L"END") return VK_END;
  if (name == L"INSERT") return VK_INSERT;
  if (name == L"DELETE") return VK_DELETE;
  if (name == L"PAGEUP") return VK_PRIOR;
  if (name == L"PAGEDOWN") return VK_NEXT;
  if (name == L"UP") return VK_UP;
  if (name == L"DOWN") return VK_DOWN;
  if (name == L"LEFT") return VK_LEFT;
  if (name == L"RIGHT") return VK_RIGHT;
  return 0;
}

std::wstring key_to_name(UINT key) {
  if ((key >= L'A' && key <= L'Z') || (key >= L'0' && key <= L'9'))
    return std::wstring(1, static_cast<wchar_t>(key));
  if (key >= VK_F1 && key <= VK_F24)
    return L"F" + std::to_wstring(key - VK_F1 + 1);
  switch (key) {
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_PRIOR: return L"PageUp";
    case VK_NEXT: return L"PageDown";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    default: return L"";
  }
}

bool modifier_down(int key) {
  return (GetAsyncKeyState(key) & 0x8000) != 0;
}

}  // namespace

std::wstring default_config_path(HMODULE module) {
  std::wstring path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(module, path.data(),
                                          static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) return L"GPUOverlay.ini";
  path.resize(length);
  const size_t slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return L"GPUOverlay.ini";
  return path.substr(0, slash + 1) + L"GPUOverlay.ini";
}

OverlayConfig load_overlay_config(const std::wstring& path) {
  OverlayConfig config;
  parse_position(read_value(path, L"Display", L"Position", L"top-left"),
                 config.position);

  const std::wstring scaleText = read_value(path, L"Display", L"Scale", L"1.0");
  wchar_t* scaleEnd = nullptr;
  const float parsedScale = std::wcstof(scaleText.c_str(), &scaleEnd);
  if (scaleEnd && *scaleEnd == L'\0' && std::isfinite(parsedScale))
    config.scale = (std::clamp)(parsedScale, 0.5f, 3.0f);

  parse_color(read_value(path, L"Display", L"TextColor", L"#00FF00"),
              config.textColor);
  parse_color(read_value(path, L"Display", L"BackgroundColor", L"#000000"),
              config.backgroundColor);

  config.showGpuUsage = read_bool(path, L"Metrics", L"GpuUsage", true);
  config.showVram = read_bool(path, L"Metrics", L"Vram", true);
  config.showFps = read_bool(path, L"Metrics", L"Fps", true);
  config.showClock = read_bool(path, L"Metrics", L"Clock", true);
  config.showTemperature = read_bool(path, L"Metrics", L"Temperature", true);

  parse_hotkey(read_value(path, L"Hotkeys", L"Toggle", L"F11"),
               config.toggleHotkey);
  parse_hotkey(read_value(path, L"Hotkeys", L"CyclePosition", L"F10"),
               config.cyclePositionHotkey);
  return config;
}

bool save_overlay_config(const std::wstring& path, const OverlayConfig& config) {
  wchar_t scale[32] = {};
  swprintf_s(scale, L"%.2f", (std::clamp)(config.scale, 0.5f, 3.0f));
  bool ok = true;
  ok = write_value(path, L"Display", L"Position",
                   position_to_string(config.position)) && ok;
  ok = write_value(path, L"Display", L"Scale", scale) && ok;
  ok = write_value(path, L"Display", L"TextColor",
                   color_to_string(config.textColor)) && ok;
  ok = write_value(path, L"Display", L"BackgroundColor",
                   color_to_string(config.backgroundColor)) && ok;
  ok = write_bool(path, L"Metrics", L"GpuUsage", config.showGpuUsage) && ok;
  ok = write_bool(path, L"Metrics", L"Vram", config.showVram) && ok;
  ok = write_bool(path, L"Metrics", L"Fps", config.showFps) && ok;
  ok = write_bool(path, L"Metrics", L"Clock", config.showClock) && ok;
  ok = write_bool(path, L"Metrics", L"Temperature",
                  config.showTemperature) && ok;
  ok = write_value(path, L"Hotkeys", L"Toggle",
                   hotkey_to_string(config.toggleHotkey)) && ok;
  ok = write_value(path, L"Hotkeys", L"CyclePosition",
                   hotkey_to_string(config.cyclePositionHotkey)) && ok;
  return ok;
}

void ensure_overlay_config(const std::wstring& path) {
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    save_overlay_config(path, OverlayConfig{});
}

std::wstring position_to_string(OverlayPosition position) {
  switch (position) {
    case OverlayPosition::TopRight: return L"top-right";
    case OverlayPosition::BottomLeft: return L"bottom-left";
    case OverlayPosition::BottomRight: return L"bottom-right";
    default: return L"top-left";
  }
}

bool parse_position(const std::wstring& text, OverlayPosition& position) {
  const std::wstring value = upper(trim(text));
  if (value == L"TOP-LEFT") position = OverlayPosition::TopLeft;
  else if (value == L"TOP-RIGHT") position = OverlayPosition::TopRight;
  else if (value == L"BOTTOM-LEFT") position = OverlayPosition::BottomLeft;
  else if (value == L"BOTTOM-RIGHT") position = OverlayPosition::BottomRight;
  else return false;
  return true;
}

std::wstring color_to_string(const OverlayColor& color) {
  wchar_t value[8] = {};
  swprintf_s(value, L"#%02X%02X%02X", color.red, color.green, color.blue);
  return value;
}

bool parse_color(const std::wstring& text, OverlayColor& color) {
  std::wstring value = trim(text);
  if (!value.empty() && value[0] == L'#') value.erase(value.begin());
  if (value.size() != 6) return false;
  if (!std::all_of(value.begin(), value.end(), [](wchar_t ch) {
        return std::iswxdigit(ch) != 0;
      }))
    return false;
  wchar_t* end = nullptr;
  const unsigned long parsed = std::wcstoul(value.c_str(), &end, 16);
  if (!end || *end != L'\0') return false;
  color.red = static_cast<BYTE>((parsed >> 16) & 0xFF);
  color.green = static_cast<BYTE>((parsed >> 8) & 0xFF);
  color.blue = static_cast<BYTE>(parsed & 0xFF);
  return true;
}

std::wstring hotkey_to_string(const OverlayHotkey& hotkey) {
  std::wstring result;
  if (hotkey.control) result += L"Ctrl+";
  if (hotkey.alt) result += L"Alt+";
  if (hotkey.shift) result += L"Shift+";
  result += key_to_name(hotkey.virtualKey);
  return result;
}

bool parse_hotkey(const std::wstring& text, OverlayHotkey& hotkey) {
  OverlayHotkey parsed;
  bool hasKey = false;
  std::wstringstream stream(text);
  std::wstring token;
  while (std::getline(stream, token, L'+')) {
    token = upper(trim(token));
    if (token.empty()) return false;
    if (token == L"CTRL" || token == L"CONTROL") {
      if (parsed.control) return false;
      parsed.control = true;
    } else if (token == L"ALT") {
      if (parsed.alt) return false;
      parsed.alt = true;
    } else if (token == L"SHIFT") {
      if (parsed.shift) return false;
      parsed.shift = true;
    } else {
      if (hasKey) return false;
      parsed.virtualKey = key_from_name(token);
      if (parsed.virtualKey == 0) return false;
      hasKey = true;
    }
  }
  if (!hasKey) return false;
  hotkey = parsed;
  return true;
}

bool hotkey_pressed(const OverlayHotkey& hotkey, HotkeyState& state) {
  if (hotkey.virtualKey == 0) return false;
  const bool modifiersMatch =
      (!hotkey.control || modifier_down(VK_CONTROL)) &&
      (!hotkey.alt || modifier_down(VK_MENU)) &&
      (!hotkey.shift || modifier_down(VK_SHIFT));
  const bool down = modifiersMatch && modifier_down(hotkey.virtualKey);
  const bool pressed = down && !state.wasDown;
  state.wasDown = down;
  return pressed;
}

OverlayPosition next_position(OverlayPosition position) {
  switch (position) {
    case OverlayPosition::TopLeft: return OverlayPosition::TopRight;
    case OverlayPosition::TopRight: return OverlayPosition::BottomRight;
    case OverlayPosition::BottomRight: return OverlayPosition::BottomLeft;
    default: return OverlayPosition::TopLeft;
  }
}

}  // namespace gpuoverlay
