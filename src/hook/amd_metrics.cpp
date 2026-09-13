#include "hook/amd_metrics.h"
#include "common/diagnostics.h"
#include "../third_party/adl_minimal.h"
#include <windows.h>
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

  int adapterIndex = 0;
  bool adlOk = false;
};

static void* __stdcall ADL_Main_Memory_Alloc_Impl(int iSize) {
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

bool AMDMetrics::init(ID3D11Device*) {
  if (!impl_) return false;
  shutdown();
#ifdef _WIN64
  impl_->adlDll = LoadLibraryW(L"atiadlxx.dll");
#else
  impl_->adlDll = LoadLibraryW(L"atiadlxy.dll");
#endif
  if (!impl_->adlDll) {
    log_last_error(LogLevel::Warning, L"AMD ADL driver library was not found");
    return false;
  }

  impl_->Main_Control_Create = reinterpret_cast<ADL2_MAIN_CONTROL_CREATE>(
      GetProcAddress(impl_->adlDll, "ADL2_Main_Control_Create"));
  impl_->Main_Control_Destroy = reinterpret_cast<ADL2_MAIN_CONTROL_DESTROY>(
      GetProcAddress(impl_->adlDll, "ADL2_Main_Control_Destroy"));
  impl_->Overdrive5_CurrentActivity_Get = reinterpret_cast<ADL2_OVERDRIVE5_CURRENTACTIVITY_GET>(
      GetProcAddress(impl_->adlDll, "ADL2_Overdrive5_CurrentActivity_Get"));
  impl_->Overdrive5_Temperature_Get = reinterpret_cast<ADL2_OVERDRIVE5_TEMPERATURE_GET>(
      GetProcAddress(impl_->adlDll, "ADL2_Overdrive5_Temperature_Get"));
  if (!impl_->Main_Control_Create || !impl_->Main_Control_Destroy ||
      !impl_->Overdrive5_CurrentActivity_Get || !impl_->Overdrive5_Temperature_Get) {
    log_message(LogLevel::Warning, L"AMD ADL telemetry functions are unavailable");
    FreeLibrary(impl_->adlDll);
    impl_->adlDll = nullptr;
    return false;
  }

  if (impl_->Main_Control_Create(ADL_Main_Memory_Alloc_Impl, 1, &impl_->context) != ADL_OK) {
    log_message(LogLevel::Warning, L"AMD ADL initialization failed");
    FreeLibrary(impl_->adlDll);
    impl_->adlDll = nullptr;
    return false;
  }

  impl_->adlOk = true;
  impl_->adapterIndex = 0;  // First AMD GPU
  log_message(LogLevel::Info, L"AMD ADL initialized");
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

void AMDMetrics::update(ID3D11Device*, GPUMetrics& out) {
  reset_vendor_metrics(out);
  out.providerName = name();

  if (impl_->adlOk && impl_->context) {
    ADLPMActivity activity = {};
    activity.iSize = sizeof(ADLPMActivity);
    if (impl_->Overdrive5_CurrentActivity_Get(impl_->context, impl_->adapterIndex, &activity) == ADL_OK) {
      out.gpuUsagePercent = activity.iActivityPercent;
      // ADL reports clocks in 10 KHz units (100 units = 1 MHz).
      out.engineClockMHz = activity.iEngineClock / 100;
      out.gpuUsageValid = true;
      out.engineClockValid = true;
    }

    ADLTemperature temp = {};
    temp.iSize = sizeof(ADLTemperature);
    if (impl_->Overdrive5_Temperature_Get(impl_->context, impl_->adapterIndex, 0, &temp) == ADL_OK) {
      out.temperatureC = temp.iTemperature / 1000;  // millidegrees -> Celsius
      out.temperatureValid = true;
    }
  }

}

}  // namespace gpuoverlay
