#pragma once

namespace gpuoverlay {

struct GPUMetrics {
  int gpuUsagePercent = 0;   // 0-100
  double vramUsageGB = 0.0;  // Dedicated VRAM used (GB)
  int engineClockMHz = 0;    // GPU core clock
  int temperatureC = 0;      // GPU temperature Celsius
  bool valid = false;
};

// Uses AMD ADL (atiadlxx.dll, shipped with Radeon drivers).
// VRAM is obtained via DXGI when device is available; other metrics via ADL.
class AMDMetrics {
public:
  AMDMetrics();
  ~AMDMetrics();

  bool init();
  void shutdown();

  // Update metrics. Pass current D3D device for VRAM via DXGI; can be null.
  void update(void* d3d11Device, GPUMetrics& out);

private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace gpuoverlay
