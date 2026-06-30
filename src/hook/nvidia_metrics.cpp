#include "hook/nvidia_metrics.h"
#include <nvapi.h>
#include <algorithm>
#include <cmath>
#include <dxgi.h>
#include <windows.h>

namespace gpuoverlay {

namespace {

bool get_adapter_luid(ID3D11Device* device, LUID& luid) {
  IDXGIDevice* dxgiDevice = nullptr;
  if (!device || FAILED(device->QueryInterface(__uuidof(IDXGIDevice),
                                               reinterpret_cast<void**>(&dxgiDevice))))
    return false;

  IDXGIAdapter* adapter = nullptr;
  const HRESULT adapterResult = dxgiDevice->GetAdapter(&adapter);
  dxgiDevice->Release();
  if (FAILED(adapterResult) || !adapter) return false;

  DXGI_ADAPTER_DESC desc = {};
  const HRESULT descResult = adapter->GetDesc(&desc);
  adapter->Release();
  if (FAILED(descResult)) return false;
  luid = desc.AdapterLuid;
  return true;
}

bool luid_equal(const LUID& left, const LUID& right) {
  return left.LowPart == right.LowPart && left.HighPart == right.HighPart;
}

}  // namespace

struct NvidiaMetrics::Impl {
  NvPhysicalGpuHandle gpu = nullptr;
  bool initialized = false;
};

NvidiaMetrics::NvidiaMetrics() : impl_(new Impl()) {}

NvidiaMetrics::~NvidiaMetrics() {
  shutdown();
  delete impl_;
  impl_ = nullptr;
}

bool NvidiaMetrics::init(ID3D11Device* device) {
  if (!impl_) return false;
  shutdown();
  if (NvAPI_Initialize() != NVAPI_OK) return false;
  impl_->initialized = true;

  NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS] = {};
  NvU32 count = 0;
  if (NvAPI_EnumPhysicalGPUs(handles, &count) != NVAPI_OK || count == 0) {
    shutdown();
    return false;
  }

  LUID targetLuid = {};
  const bool hasTargetLuid = get_adapter_luid(device, targetLuid);
  for (NvU32 index = 0; index < count; ++index) {
    LUID candidateLuid = {};
#pragma warning(push)
#pragma warning(disable : 4996)
    const NvAPI_Status luidResult =
        NvAPI_GPU_GetAdapterIdFromPhysicalGpu(handles[index], &candidateLuid);
#pragma warning(pop)
    if (hasTargetLuid && luidResult == NVAPI_OK &&
        luid_equal(targetLuid, candidateLuid)) {
      impl_->gpu = handles[index];
      break;
    }
  }

  if (!impl_->gpu) impl_->gpu = handles[0];
  return true;
}

void NvidiaMetrics::shutdown() {
  if (!impl_) return;
  impl_->gpu = nullptr;
  if (impl_->initialized) NvAPI_Unload();
  impl_->initialized = false;
}

void NvidiaMetrics::update(ID3D11Device*, GPUMetrics& out) {
  reset_vendor_metrics(out);
  out.providerName = name();
  if (!impl_ || !impl_->initialized || !impl_->gpu) return;

  NV_GPU_DYNAMIC_PSTATES_INFO_EX utilization = {};
  utilization.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
  if (NvAPI_GPU_GetDynamicPstatesInfoEx(impl_->gpu, &utilization) == NVAPI_OK &&
      utilization.utilization[0].bIsPresent) {
    out.gpuUsagePercent = static_cast<int>((std::min<NvU32>)(
        utilization.utilization[0].percentage, static_cast<NvU32>(100)));
    out.gpuUsageValid = true;
  }

  NV_GPU_CLOCK_FREQUENCIES clocks = {};
  clocks.version = NV_GPU_CLOCK_FREQUENCIES_VER;
  clocks.ClockType = NV_GPU_CLOCK_FREQUENCIES_CURRENT_FREQ;
  if (NvAPI_GPU_GetAllClockFrequencies(impl_->gpu, &clocks) == NVAPI_OK &&
      clocks.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].bIsPresent) {
    out.engineClockMHz = static_cast<int>(std::lround(
        clocks.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].frequency / 1000.0));
    out.engineClockValid = true;
  }

  NV_GPU_THERMAL_SETTINGS thermal = {};
  thermal.version = NV_GPU_THERMAL_SETTINGS_VER;
  if (NvAPI_GPU_GetThermalSettings(impl_->gpu, NVAPI_THERMAL_TARGET_ALL,
                                   &thermal) == NVAPI_OK) {
    for (NvU32 index = 0; index < thermal.count; ++index) {
      if (thermal.sensor[index].target == NVAPI_THERMAL_TARGET_GPU ||
          (index == 0 && !out.temperatureValid)) {
        out.temperatureC = thermal.sensor[index].currentTemp;
        out.temperatureValid = true;
        if (thermal.sensor[index].target == NVAPI_THERMAL_TARGET_GPU) break;
      }
    }
  }
}

}  // namespace gpuoverlay
