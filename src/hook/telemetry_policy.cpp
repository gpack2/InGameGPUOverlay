#include "hook/telemetry_policy.h"

namespace gpuoverlay {

TelemetryProviderKind provider_kind_from_vendor_id(uint32_t vendorId) {
  switch (vendorId) {
    case 0x1002: return TelemetryProviderKind::AmdAdl;
    case 0x10DE: return TelemetryProviderKind::NvidiaNvapi;
    case 0x8086: return TelemetryProviderKind::IntelIgcl;
    default: return TelemetryProviderKind::Dxgi;
  }
}

const wchar_t* provider_kind_name(TelemetryProviderKind kind) {
  switch (kind) {
    case TelemetryProviderKind::AmdAdl: return L"AMD ADL";
    case TelemetryProviderKind::NvidiaNvapi: return L"NVIDIA NVAPI";
    case TelemetryProviderKind::IntelIgcl: return L"Intel IGCL";
    default: return L"DXGI";
  }
}

std::unique_ptr<TelemetryProvider> initialize_provider_or_fallback(
    std::unique_ptr<TelemetryProvider> provider,
    std::unique_ptr<TelemetryProvider> fallback, ID3D11Device* device) {
  if (provider && provider->init(device)) return provider;
  if (provider) provider->shutdown();
  if (fallback && fallback->init(device)) return fallback;
  if (fallback) fallback->shutdown();
  return nullptr;
}

}  // namespace gpuoverlay
