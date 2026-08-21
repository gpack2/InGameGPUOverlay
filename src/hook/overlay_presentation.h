#pragma once

#include "common/overlay_config.h"
#include "hook/telemetry_provider.h"
#include <string>

namespace gpuoverlay {

inline constexpr int kOverlayWidth = 320;
inline constexpr int kOverlayHeight = 140;
inline constexpr int kOverlayPadding = 8;
inline constexpr int kOverlayLineHeight = 22;

struct OverlayText {
  std::wstring value;
  int lineCount = 0;
};

struct OverlayBounds {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  float sourceHeight = 0.0f;
};

OverlayText format_overlay_text(const GPUMetrics& metrics, int fps,
                                const OverlayConfig& config);
OverlayBounds calculate_overlay_bounds(int surfaceWidth, int surfaceHeight,
                                       int lineCount,
                                       const OverlayConfig& config);

}  // namespace gpuoverlay
