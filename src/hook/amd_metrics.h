#pragma once

#include "hook/telemetry_provider.h"

namespace gpuoverlay {

// Uses AMD ADL (atiadlxx.dll, shipped with Radeon drivers).
class AMDMetrics final : public TelemetryProvider {
public:
  AMDMetrics();
  ~AMDMetrics() override;

  bool init(ID3D11Device* device) override;
  void shutdown() override;
  void update(ID3D11Device* device, GPUMetrics& out) override;
  const wchar_t* name() const override { return L"AMD ADL"; }

private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace gpuoverlay
