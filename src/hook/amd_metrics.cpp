#include "hook/amd_metrics.h"
#include "../third_party/adl_minimal.h"
#include <windows.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d11.h>
#include <cstring>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

namespace gpuoverlay {

struct AMDMetrics::Impl {
  HMODULE adlDll = nullptr;
  ADL_CONTEXT_HANDLE context = nullptr;

  ADL2_MAIN_CONTROL_CREATE Main_Control_Create = nullptr;
  ADL2_MAIN_CONTROL_DESTROY Main_Control_Destroy = nullptr;
  ADL2_OVERDRIVE5_CURRENTACTIVITY_GET Overdrive5_CurrentActivity_Get = nullptr;
  ADL2_OVERDRIVE5_TEMPERATURE_GET Overdrive5_Temperature_Get = nullptr;
  ADL_MAIN_MEMORY_ALLOC Main_Memory_Alloc = nullptr;

  int adapterIndex = 0;
  bool adlOk = false;
};

static void* ADL_Main_Memory_Alloc_Impl(int iSize) {
  return malloc(static_cast<size_t>(iSize));
}

AMDMetrics::AMDMetrics() {
  impl_ = new Impl();
}

AMDMetrics::~AMDMetrics() {
  shutdown();
  delete impl_;
  impl_ = nullptr;
}

bool AMDMetrics::init() {
  if (!impl_) return false;
#ifdef _WIN64
  impl_->adlDll = LoadLibraryW(L"atiadlxx.dll");
#else
  impl_->adlDll = LoadLibraryW(L"atiadlxy.dll");
#endif
  if (!impl_->adlDll) return false;

  impl_->Main_Control_Create = reinterpret_cast<ADL2_MAIN_CONTROL_CREATE>(
      GetProcAddress(impl_->adlDll, "ADL2_Main_Control_Create"));
  impl_->Main_Control_Destroy = reinterpret_cast<ADL2_MAIN_CONTROL_DESTROY>(
      GetProcAddress(impl_->adlDll, "ADL2_Main_Control_Destroy"));
  impl_->Overdrive5_CurrentActivity_Get = reinterpret_cast<ADL2_OVERDRIVE5_CURRENTACTIVITY_GET>(
      GetProcAddress(impl_->adlDll, "ADL2_Overdrive5_CurrentActivity_Get"));
  impl_->Overdrive5_Temperature_Get = reinterpret_cast<ADL2_OVERDRIVE5_TEMPERATURE_GET>(
      GetProcAddress(impl_->adlDll, "ADL2_Overdrive5_Temperature_Get"));
  impl_->Main_Memory_Alloc = reinterpret_cast<ADL_MAIN_MEMORY_ALLOC>(
      GetProcAddress(impl_->adlDll, "ADL_Main_Memory_Alloc"));

  if (!impl_->Main_Control_Create || !impl_->Main_Control_Destroy ||
      !impl_->Overdrive5_CurrentActivity_Get || !impl_->Overdrive5_Temperature_Get) {
    FreeLibrary(impl_->adlDll);
    impl_->adlDll = nullptr;
    return false;
  }

  if (impl_->Main_Control_Create(ADL_Main_Memory_Alloc_Impl, 1, &impl_->context) != ADL_OK) {
    FreeLibrary(impl_->adlDll);
    impl_->adlDll = nullptr;
    return false;
  }

  impl_->adlOk = true;
  impl_->adapterIndex = 0;  // First AMD GPU
  return true;
}

void AMDMetrics::shutdown() {
  if (!impl_) return;
  if (impl_->context && impl_->Main_Control_Destroy) {
    impl_->Main_Control_Destroy(impl_->context);
    impl_->context = nullptr;
  }
  if (impl_->adlDll) {
    FreeLibrary(impl_->adlDll);
    impl_->adlDll = nullptr;
  }
  impl_->adlOk = false;
}

static double getVramUsageGB(ID3D11Device* device) {
  if (!device) return 0.0;
  IDXGIDevice* dxgiDevice = nullptr;
  if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))))
    return 0.0;
  IDXGIAdapter* adapter = nullptr;
  if (FAILED(dxgiDevice->GetAdapter(&adapter))) {
    dxgiDevice->Release();
    return 0.0;
  }
  IDXGIAdapter3* adapter3 = nullptr;
  HRESULT hr = adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter3));
  adapter->Release();
  dxgiDevice->Release();
  if (FAILED(hr) || !adapter3) return 0.0;

  DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
  hr = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
  adapter3->Release();
  if (FAILED(hr)) return 0.0;

  // CurrentUsage is in bytes
  return static_cast<double>(info.CurrentUsage) / (1024.0 * 1024.0 * 1024.0);
}

void AMDMetrics::update(void* d3d11Device, GPUMetrics& out) {
  out.valid = false;
  out.gpuUsagePercent = 0;
  out.vramUsageGB = 0.0;
  out.engineClockMHz = 0;
  out.temperatureC = 0;

  if (impl_->adlOk && impl_->context) {
    ADLPMActivity activity = {};
    activity.iSize = sizeof(ADLPMActivity);
    if (impl_->Overdrive5_CurrentActivity_Get(impl_->context, impl_->adapterIndex, &activity) == ADL_OK) {
      out.gpuUsagePercent = activity.iActivityPercent;
      // ADL engine/memory clock: typically in 10 KHz (10000 = 1 MHz)
      out.engineClockMHz = activity.iEngineClock / 100;
    }

    ADLTemperature temp = {};
    temp.iSize = sizeof(ADLTemperature);
    if (impl_->Overdrive5_Temperature_Get(impl_->context, impl_->adapterIndex, 0, &temp) == ADL_OK) {
      out.temperatureC = temp.iTemperature / 1000;  // millidegrees -> Celsius
    }

    out.valid = true;
  }

  if (d3d11Device) {
    out.vramUsageGB = getVramUsageGB(static_cast<ID3D11Device*>(d3d11Device));
  }
}

}  // namespace gpuoverlay
