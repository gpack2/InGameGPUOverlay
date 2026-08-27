#include "hook/telemetry_provider.h"
#include "common/diagnostics.h"
#include "hook/amd_metrics.h"
#include "hook/intel_metrics.h"
#include "hook/nvidia_metrics.h"
#include "hook/telemetry_policy.h"
#include <dxgi1_4.h>
#include <iomanip>
#include <sstream>
#include <utility>

namespace gpuoverlay {

namespace {

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
  if (!get_adapter_desc(device, desc)) {
    log_message(LogLevel::Warning,
                L"Could not read the DirectX adapter description; using DXGI");
  } else {
    std::wostringstream message;
    message << L"DirectX adapter: " << desc.Description << L" (vendor 0x"
            << std::hex << std::uppercase << desc.VendorId << L")";
    log_message(LogLevel::Info, message.str());
  }

  std::unique_ptr<TelemetryProvider> provider;
  const TelemetryProviderKind kind = provider_kind_from_vendor_id(desc.VendorId);
  switch (kind) {
    case TelemetryProviderKind::AmdAdl:
      provider = std::make_unique<AMDMetrics>();
      break;
    case TelemetryProviderKind::NvidiaNvapi:
      provider = std::make_unique<NvidiaMetrics>();
      break;
    case TelemetryProviderKind::IntelIgcl:
      provider = std::make_unique<IntelMetrics>();
      break;
    default:
      provider = std::make_unique<DxgiMetrics>();
      break;
  }

  const bool isFallback = kind == TelemetryProviderKind::Dxgi;
  const wchar_t* selectedName = provider_kind_name(kind);
  std::unique_ptr<TelemetryProvider> fallback;
  if (!isFallback) fallback = std::make_unique<DxgiMetrics>();
  std::unique_ptr<TelemetryProvider> selected = initialize_provider_or_fallback(
      std::move(provider), std::move(fallback), device);
  if (!selected) {
    log_message(LogLevel::Error, L"No telemetry provider could be initialized");
    return nullptr;
  }
  if (!isFallback && std::wstring(selected->name()) == L"DXGI") {
    log_message(LogLevel::Warning,
                std::wstring(selectedName) + L" unavailable; using DXGI fallback");
  } else {
    log_message(LogLevel::Info,
                std::wstring(L"Telemetry provider: ") + selected->name());
  }
  return selected;
}

}  // namespace gpuoverlay
