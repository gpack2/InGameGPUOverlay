#pragma once

#include "hook/telemetry_provider.h"

namespace gpuoverlay {

class NvidiaMetrics final : public TelemetryProvider {
public:
  NvidiaMetrics();
  ~NvidiaMetrics() override;

  bool init(ID3D11Device* device) override;
  void shutdown() override;
  void update(ID3D11Device* device, GPUMetrics& out) override;
  const wchar_t* name() const override { return L"NVIDIA NVAPI"; }

private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace gpuoverlay
