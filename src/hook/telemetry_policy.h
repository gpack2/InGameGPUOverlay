#pragma once

#include "hook/telemetry_provider.h"
#include <cstdint>

namespace gpuoverlay {

enum class TelemetryProviderKind {
  Dxgi,
  AmdAdl,
  NvidiaNvapi,
  IntelIgcl,
};

TelemetryProviderKind provider_kind_from_vendor_id(uint32_t vendorId);
const wchar_t* provider_kind_name(TelemetryProviderKind kind);
std::unique_ptr<TelemetryProvider> initialize_provider_or_fallback(
    std::unique_ptr<TelemetryProvider> provider,
    std::unique_ptr<TelemetryProvider> fallback, ID3D11Device* device);

}  // namespace gpuoverlay
