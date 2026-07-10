#include "hook/telemetry_provider.h"
#include "hook/amd_metrics.h"
#include "hook/intel_metrics.h"
#include "hook/nvidia_metrics.h"
#include <dxgi1_4.h>

namespace gpuoverlay {

namespace {

constexpr UINT kAmdVendorId = 0x1002;
constexpr UINT kNvidiaVendorId = 0x10DE;
constexpr UINT kIntelVendorId = 0x8086;

class DxgiMetrics final : public TelemetryProvider {
public:
  bool init(ID3D11Device*) override { return true; }
  void shutdown() override {}
  void update(ID3D11Device*, GPUMetrics& out) override {
    reset_vendor_metrics(out);
    out.providerName = L"DXGI";
  }
  const wchar_t* name() const override { return L"DXGI"; }
};

bool get_adapter_desc(ID3D11Device* device, DXGI_ADAPTER_DESC& desc) {
  if (!device) return false;
  IDXGIDevice* dxgiDevice = nullptr;
  if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice),
                                    reinterpret_cast<void**>(&dxgiDevice))))
    return false;

  IDXGIAdapter* adapter = nullptr;
  const HRESULT adapterResult = dxgiDevice->GetAdapter(&adapter);
  dxgiDevice->Release();
  if (FAILED(adapterResult) || !adapter) return false;

  const HRESULT descResult = adapter->GetDesc(&desc);
  adapter->Release();
  return SUCCEEDED(descResult);
}

}  // namespace

void reset_vendor_metrics(GPUMetrics& out) {
  out.gpuUsagePercent = 0;
  out.engineClockMHz = 0;
  out.temperatureC = 0;
  out.gpuUsageValid = false;
  out.engineClockValid = false;
  out.temperatureValid = false;
}

void update_dxgi_memory(ID3D11Device* device, GPUMetrics& out) {
  out.vramUsageGB = 0.0;
  out.vramUsageValid = false;
  if (!device) return;

  IDXGIDevice* dxgiDevice = nullptr;
  if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice),
                                    reinterpret_cast<void**>(&dxgiDevice))))
    return;

  IDXGIAdapter* adapter = nullptr;
  if (FAILED(dxgiDevice->GetAdapter(&adapter))) {
    dxgiDevice->Release();
    return;
  }

  IDXGIAdapter3* adapter3 = nullptr;
  const HRESULT adapter3Result = adapter->QueryInterface(
      __uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter3));
  adapter->Release();
  dxgiDevice->Release();
  if (FAILED(adapter3Result) || !adapter3) return;

  DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
  const HRESULT memoryResult = adapter3->QueryVideoMemoryInfo(
      0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
  adapter3->Release();
  if (FAILED(memoryResult)) return;

  out.vramUsageGB = static_cast<double>(info.CurrentUsage) /
                    (1024.0 * 1024.0 * 1024.0);
  out.vramUsageValid = true;
}

std::unique_ptr<TelemetryProvider> create_telemetry_provider(ID3D11Device* device) {
  DXGI_ADAPTER_DESC desc = {};
  get_adapter_desc(device, desc);

  std::unique_ptr<TelemetryProvider> provider;
  switch (desc.VendorId) {
    case kAmdVendorId:
      provider = std::make_unique<AMDMetrics>();
      break;
    case kNvidiaVendorId:
      provider = std::make_unique<NvidiaMetrics>();
      break;
    case kIntelVendorId:
      provider = std::make_unique<IntelMetrics>();
      break;
    default:
      provider = std::make_unique<DxgiMetrics>();
      break;
  }

  if (provider->init(device)) return provider;
  provider->shutdown();
  provider = std::make_unique<DxgiMetrics>();
  provider->init(device);
  return provider;
}

}  // namespace gpuoverlay
