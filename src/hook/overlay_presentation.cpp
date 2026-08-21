#include "hook/overlay_presentation.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace gpuoverlay {

OverlayText format_overlay_text(const GPUMetrics& metrics, int fps,
                                const OverlayConfig& config) {
  std::wostringstream text;
  int lineCount = 0;
  const auto addLine = [&](const std::wstring& line) {
    if (lineCount > 0) text << L'\n';
    text << line;
    ++lineCount;
  };

  if (config.showGpuUsage) {
    std::wostringstream line;
    line << metrics.providerName << L": ";
    if (metrics.gpuUsageValid) line << metrics.gpuUsagePercent << L'%';
    else line << L"N/A";
    addLine(line.str());
  }
  if (config.showVram) {
    std::wostringstream line;
    line << L"VRAM: ";
    if (metrics.vramUsageValid)
      line << std::fixed << std::setprecision(2) << metrics.vramUsageGB << L" GB";
    else
      line << L"N/A";
    addLine(line.str());
  }
  if (config.showFps) addLine(L"FPS: " + std::to_wstring(fps));
  if (config.showClock) {
    std::wostringstream line;
    line << L"Clock: ";
    if (metrics.engineClockValid) line << metrics.engineClockMHz << L" MHz";
    else line << L"N/A";
    addLine(line.str());
  }
  if (config.showTemperature) {
    std::wostringstream line;
    line << L"Temp: ";
    if (metrics.temperatureValid) line << metrics.temperatureC << L" C";
    else line << L"N/A";
    addLine(line.str());
  }
  return {text.str(), lineCount};
}

OverlayBounds calculate_overlay_bounds(int surfaceWidth, int surfaceHeight,
                                       int lineCount,
                                       const OverlayConfig& config) {
  const int contentHeight = kOverlayPadding * 2 +
                            (std::max)(0, lineCount) * kOverlayLineHeight;
  const float scale = (std::clamp)(config.scale, 0.5f, 3.0f);
  OverlayBounds bounds;
  bounds.width = static_cast<int>(std::lround(kOverlayWidth * scale));
  bounds.height = static_cast<int>(std::lround(contentHeight * scale));
  const int margin = static_cast<int>(std::lround(kOverlayPadding * scale));
  bounds.x = margin;
  bounds.y = margin;
  if (config.position == OverlayPosition::TopRight ||
      config.position == OverlayPosition::BottomRight)
    bounds.x = (std::max)(0, surfaceWidth - bounds.width - margin);
  if (config.position == OverlayPosition::BottomLeft ||
      config.position == OverlayPosition::BottomRight)
    bounds.y = (std::max)(0, surfaceHeight - bounds.height - margin);
  bounds.sourceHeight = static_cast<float>(contentHeight) / kOverlayHeight;
  return bounds;
}

}  // namespace gpuoverlay
