#include "common/overlay_config.h"

using namespace gpuoverlay;

int main() {
  OverlayPosition position = OverlayPosition::TopLeft;
  if (!parse_position(L" bottom-right ", position)) return 1;
  if (position != OverlayPosition::BottomRight) return 2;
  if (parse_position(L"center", position)) return 3;
  if (next_position(OverlayPosition::TopLeft) != OverlayPosition::TopRight)
    return 4;
  if (next_position(OverlayPosition::TopRight) != OverlayPosition::BottomRight)
    return 5;
  if (next_position(OverlayPosition::BottomRight) != OverlayPosition::BottomLeft)
    return 6;
  if (next_position(OverlayPosition::BottomLeft) != OverlayPosition::TopLeft)
    return 7;

  OverlayColor color;
  if (!parse_color(L"#12aBcF", color)) return 8;
  const OverlayColor expectedColor{0x12, 0xAB, 0xCF};
  if (color != expectedColor) return 9;
  if (color_to_string(color) != L"#12ABCF") return 10;
  if (parse_color(L"-00001", color)) return 11;
  if (parse_color(L"#12345", color)) return 12;

  OverlayHotkey hotkey;
  if (!parse_hotkey(L"Ctrl+Shift+F12", hotkey)) return 13;
  if (!hotkey.control || !hotkey.shift || hotkey.alt) return 14;
  if (hotkey.virtualKey != VK_F12) return 15;
  if (hotkey_to_string(hotkey) != L"Ctrl+Shift+F12") return 16;
  if (!parse_hotkey(L"Alt+PageUp", hotkey)) return 17;
  if (!hotkey.alt || hotkey.virtualKey != VK_PRIOR) return 18;
  if (parse_hotkey(L"Bogus+F11", hotkey)) return 19;
  if (parse_hotkey(L"Ctrl+Ctrl+F11", hotkey)) return 20;
  if (parse_hotkey(L"Ctrl", hotkey)) return 21;
  return 0;
}
