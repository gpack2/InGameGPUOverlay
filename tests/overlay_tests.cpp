#include "hook/overlay_presentation.h"
#include "hook/telemetry_policy.h"
#include <cmath>
#include <memory>

using namespace gpuoverlay;

namespace {

struct MockState {
  int initCalls = 0;
  int shutdownCalls = 0;
};

class MockProvider final : public TelemetryProvider {
public:
  MockProvider(const wchar_t* providerName, bool initResult,
               std::shared_ptr<MockState> state)
      : providerName_(providerName), initResult_(initResult), state_(state) {}

  bool init(ID3D11Device*) override {
    ++state_->initCalls;
    return initResult_;
  }
  void shutdown() override { ++state_->shutdownCalls; }
  void update(ID3D11Device*, GPUMetrics&) override {}
  const wchar_t* name() const override { return providerName_; }

private:
  const wchar_t* providerName_;
  bool initResult_;
  std::shared_ptr<MockState> state_;
};

bool close_to(float left, float right) {
  return std::fabs(left - right) < 0.001f;
}

}  // namespace

int main() {
  if (provider_kind_from_vendor_id(0x1002) !=
      TelemetryProviderKind::AmdAdl)
    return 1;
  if (provider_kind_from_vendor_id(0x10DE) !=
      TelemetryProviderKind::NvidiaNvapi)
    return 2;
  if (provider_kind_from_vendor_id(0x8086) !=
      TelemetryProviderKind::IntelIgcl)
    return 3;
  if (provider_kind_from_vendor_id(0x1234) != TelemetryProviderKind::Dxgi)
    return 4;

  auto selectedState = std::make_shared<MockState>();
  auto unusedFallbackState = std::make_shared<MockState>();
  auto selected = initialize_provider_or_fallback(
      std::make_unique<MockProvider>(L"selected", true, selectedState),
      std::make_unique<MockProvider>(L"fallback", true, unusedFallbackState),
      nullptr);
  if (!selected || std::wstring(selected->name()) != L"selected") return 5;
  if (selectedState->initCalls != 1 || selectedState->shutdownCalls != 0)
    return 6;
  if (unusedFallbackState->initCalls != 0) return 7;

  auto failedState = std::make_shared<MockState>();
  auto fallbackState = std::make_shared<MockState>();
  auto fallback = initialize_provider_or_fallback(
      std::make_unique<MockProvider>(L"failed", false, failedState),
      std::make_unique<MockProvider>(L"fallback", true, fallbackState), nullptr);
  if (!fallback || std::wstring(fallback->name()) != L"fallback") return 8;
  if (failedState->initCalls != 1 || failedState->shutdownCalls != 1) return 9;
  if (fallbackState->initCalls != 1) return 10;

  GPUMetrics metrics;
  metrics.providerName = L"NVIDIA NVAPI";
  metrics.gpuUsagePercent = 73;
  metrics.vramUsageGB = 4.25;
  metrics.engineClockMHz = 1980;
  metrics.temperatureC = 64;
  metrics.gpuUsageValid = true;
  metrics.vramUsageValid = true;
  metrics.engineClockValid = true;
  metrics.temperatureValid = true;
  OverlayConfig config;
  const OverlayText text = format_overlay_text(metrics, 144, config);
  if (text.lineCount != 5) return 11;
  if (text.value != L"NVIDIA NVAPI: 73%\nVRAM: 4.25 GB\nFPS: 144\n"
                    L"Clock: 1980 MHz\nTemp: 64 C")
    return 12;

  config.showGpuUsage = false;
  config.showVram = false;
  config.showClock = false;
  config.showTemperature = false;
  const OverlayText fpsOnly = format_overlay_text(metrics, 60, config);
  if (fpsOnly.lineCount != 1 || fpsOnly.value != L"FPS: 60") return 13;

  config = OverlayConfig{};
  OverlayBounds bounds = calculate_overlay_bounds(1920, 1080, 5, config);
  if (bounds.x != 8 || bounds.y != 8 || bounds.width != 320 ||
      bounds.height != 126 || !close_to(bounds.sourceHeight, 0.9f))
    return 14;

  config.position = OverlayPosition::BottomRight;
  bounds = calculate_overlay_bounds(1920, 1080, 5, config);
  if (bounds.x != 1592 || bounds.y != 946) return 15;

  config.position = OverlayPosition::BottomLeft;
  config.scale = 2.0f;
  bounds = calculate_overlay_bounds(1920, 1080, 5, config);
  if (bounds.x != 16 || bounds.y != 812 || bounds.width != 640 ||
      bounds.height != 252)
    return 16;
  return 0;
}
