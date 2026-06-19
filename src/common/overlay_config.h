#pragma once

#include <windows.h>
#include <string>

namespace gpuoverlay {

enum class OverlayPosition {
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight,
};

struct OverlayColor {
  BYTE red = 0;
  BYTE green = 0;
  BYTE blue = 0;

  bool operator==(const OverlayColor& other) const {
    return red == other.red && green == other.green && blue == other.blue;
  }
  bool operator!=(const OverlayColor& other) const { return !(*this == other); }
  COLORREF colorref() const { return RGB(red, green, blue); }
};

struct OverlayHotkey {
  UINT virtualKey = 0;
  bool control = false;
  bool alt = false;
  bool shift = false;
};

struct HotkeyState {
  bool wasDown = false;
};

struct OverlayConfig {
  OverlayPosition position = OverlayPosition::TopLeft;
  float scale = 1.0f;
  OverlayColor textColor{0, 255, 0};
  OverlayColor backgroundColor{0, 0, 0};
  bool showGpuUsage = true;
  bool showVram = true;
  bool showFps = true;
  bool showClock = true;
  bool showTemperature = true;
  OverlayHotkey toggleHotkey{VK_F11, false, false, false};
  OverlayHotkey cyclePositionHotkey{VK_F10, false, false, false};
};

std::wstring default_config_path(HMODULE module = nullptr);
OverlayConfig load_overlay_config(const std::wstring& path);
bool save_overlay_config(const std::wstring& path, const OverlayConfig& config);
void ensure_overlay_config(const std::wstring& path);

std::wstring position_to_string(OverlayPosition position);
bool parse_position(const std::wstring& text, OverlayPosition& position);
std::wstring color_to_string(const OverlayColor& color);
bool parse_color(const std::wstring& text, OverlayColor& color);
std::wstring hotkey_to_string(const OverlayHotkey& hotkey);
bool parse_hotkey(const std::wstring& text, OverlayHotkey& hotkey);
bool hotkey_pressed(const OverlayHotkey& hotkey, HotkeyState& state);
OverlayPosition next_position(OverlayPosition position);

}  // namespace gpuoverlay
