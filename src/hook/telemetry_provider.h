#pragma once

#include <d3d11.h>
#include <memory>
#include <string>

namespace gpuoverlay {

struct GPUMetrics {
  int gpuUsagePercent = 0;
  double vramUsageGB = 0.0;
  int engineClockMHz = 0;
  int temperatureC = 0;
  bool gpuUsageValid = false;
  bool vramUsageValid = false;
  bool engineClockValid = false;
  bool temperatureValid = false;
  std::wstring providerName = L"DXGI";
};

class TelemetryProvider {
public:
  virtual ~TelemetryProvider() = default;
  virtual bool init(ID3D11Device* device) = 0;
  virtual void shutdown() = 0;
  virtual void update(ID3D11Device* device, GPUMetrics& out) = 0;
  virtual const wchar_t* name() const = 0;
};

std::unique_ptr<TelemetryProvider> create_telemetry_provider(ID3D11Device* device);
void reset_vendor_metrics(GPUMetrics& out);
void update_dxgi_memory(ID3D11Device* device, GPUMetrics& out);

}  // namespace gpuoverlay
