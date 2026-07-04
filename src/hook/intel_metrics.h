#pragma once

#include "hook/telemetry_provider.h"

namespace gpuoverlay {

class IntelMetrics final : public TelemetryProvider {
public:
  IntelMetrics();
  ~IntelMetrics() override;

  bool init(ID3D11Device* device) override;
  void shutdown() override;
  void update(ID3D11Device* device, GPUMetrics& out) override;
  const wchar_t* name() const override { return L"Intel IGCL"; }

private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace gpuoverlay
