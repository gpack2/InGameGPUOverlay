#include "hook/intel_metrics.h"
#include <algorithm>
#include <cmath>
#include <dxgi.h>
#include <vector>
#include <windows.h>

#if defined(GPUOVERLAY_HAS_IGCL) && defined(_WIN64)
#include <igcl_api.h>
#endif

namespace gpuoverlay {

#if defined(GPUOVERLAY_HAS_IGCL) && defined(_WIN64)

namespace {

template <typename T>
bool load_function(HMODULE module, const char* name, T& function) {
  function = reinterpret_cast<T>(GetProcAddress(module, name));
  return function != nullptr;
}

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

struct IntelMetrics::Impl {
  HMODULE module = nullptr;
  ctl_api_handle_t api = nullptr;
  ctl_device_adapter_handle_t device = nullptr;
  ctl_engine_handle_t engine = nullptr;
  ctl_freq_handle_t frequency = nullptr;
  ctl_temp_handle_t temperature = nullptr;
  ctl_engine_stats_t previousEngineStats = {};
  bool hasPreviousEngineStats = false;

  ctl_pfnInit_t pInit = nullptr;
  ctl_pfnClose_t pClose = nullptr;
  ctl_pfnEnumerateDevices_t pEnumerateDevices = nullptr;
  ctl_pfnGetDeviceProperties_t pGetDeviceProperties = nullptr;
  ctl_pfnEnumEngineGroups_t pEnumEngineGroups = nullptr;
  ctl_pfnEngineGetProperties_t pEngineGetProperties = nullptr;
  ctl_pfnEngineGetActivity_t pEngineGetActivity = nullptr;
  ctl_pfnEnumFrequencyDomains_t pEnumFrequencyDomains = nullptr;
  ctl_pfnFrequencyGetProperties_t pFrequencyGetProperties = nullptr;
  ctl_pfnFrequencyGetState_t pFrequencyGetState = nullptr;
  ctl_pfnEnumTemperatureSensors_t pEnumTemperatureSensors = nullptr;
  ctl_pfnTemperatureGetProperties_t pTemperatureGetProperties = nullptr;
  ctl_pfnTemperatureGetState_t pTemperatureGetState = nullptr;
};

#else

struct IntelMetrics::Impl {};

#endif

IntelMetrics::IntelMetrics() : impl_(new Impl()) {}

IntelMetrics::~IntelMetrics() {
  shutdown();
  delete impl_;
  impl_ = nullptr;
}

bool IntelMetrics::init(ID3D11Device* d3dDevice) {
#if defined(GPUOVERLAY_HAS_IGCL) && defined(_WIN64)
  if (!impl_) return false;
  shutdown();
  impl_->module = LoadLibraryExW(L"ControlLib.dll", nullptr,
                                 LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!impl_->module) return false;

  const bool functionsLoaded =
      load_function(impl_->module, "ctlInit", impl_->pInit) &&
      load_function(impl_->module, "ctlClose", impl_->pClose) &&
      load_function(impl_->module, "ctlEnumerateDevices", impl_->pEnumerateDevices) &&
      load_function(impl_->module, "ctlGetDeviceProperties", impl_->pGetDeviceProperties) &&
      load_function(impl_->module, "ctlEnumEngineGroups", impl_->pEnumEngineGroups) &&
      load_function(impl_->module, "ctlEngineGetProperties", impl_->pEngineGetProperties) &&
      load_function(impl_->module, "ctlEngineGetActivity", impl_->pEngineGetActivity) &&
      load_function(impl_->module, "ctlEnumFrequencyDomains", impl_->pEnumFrequencyDomains) &&
      load_function(impl_->module, "ctlFrequencyGetProperties", impl_->pFrequencyGetProperties) &&
      load_function(impl_->module, "ctlFrequencyGetState", impl_->pFrequencyGetState) &&
      load_function(impl_->module, "ctlEnumTemperatureSensors", impl_->pEnumTemperatureSensors) &&
      load_function(impl_->module, "ctlTemperatureGetProperties", impl_->pTemperatureGetProperties) &&
      load_function(impl_->module, "ctlTemperatureGetState", impl_->pTemperatureGetState);
  if (!functionsLoaded) {
    shutdown();
    return false;
  }

  ctl_init_args_t initArgs = {};
  initArgs.Size = sizeof(initArgs);
  initArgs.AppVersion = CTL_IMPL_VERSION;
  initArgs.flags = CTL_INIT_FLAG_USE_LEVEL_ZERO;
  if (impl_->pInit(&initArgs, &impl_->api) != CTL_RESULT_SUCCESS || !impl_->api) {
    shutdown();
    return false;
  }

  uint32_t deviceCount = 0;
  if (impl_->pEnumerateDevices(impl_->api, &deviceCount, nullptr) !=
          CTL_RESULT_SUCCESS ||
      deviceCount == 0) {
    shutdown();
    return false;
  }
  std::vector<ctl_device_adapter_handle_t> devices(deviceCount);
  if (impl_->pEnumerateDevices(impl_->api, &deviceCount, devices.data()) !=
      CTL_RESULT_SUCCESS) {
    shutdown();
    return false;
  }

  LUID targetLuid = {};
  const bool hasTargetLuid = get_adapter_luid(d3dDevice, targetLuid);
  for (ctl_device_adapter_handle_t candidate : devices) {
    LUID candidateLuid = {};
    ctl_device_adapter_properties_t properties = {};
    properties.Size = sizeof(properties);
    properties.Version = 2;
    properties.pDeviceID = &candidateLuid;
    properties.device_id_size = sizeof(candidateLuid);
    if (impl_->pGetDeviceProperties(candidate, &properties) == CTL_RESULT_SUCCESS &&
        properties.pci_vendor_id == 0x8086 &&
        (!hasTargetLuid || luid_equal(targetLuid, candidateLuid))) {
      impl_->device = candidate;
      break;
    }
  }
  if (!impl_->device) {
    shutdown();
    return false;
  }

  uint32_t engineCount = 0;
  if (impl_->pEnumEngineGroups(impl_->device, &engineCount, nullptr) ==
          CTL_RESULT_SUCCESS &&
      engineCount > 0) {
    std::vector<ctl_engine_handle_t> engines(engineCount);
    if (impl_->pEnumEngineGroups(impl_->device, &engineCount, engines.data()) ==
        CTL_RESULT_SUCCESS) {
      for (ctl_engine_handle_t engine : engines) {
        ctl_engine_properties_t properties = {};
        properties.Size = sizeof(properties);
        if (impl_->pEngineGetProperties(engine, &properties) == CTL_RESULT_SUCCESS) {
          if (!impl_->engine || properties.type == CTL_ENGINE_GROUP_GT)
            impl_->engine = engine;
          if (properties.type == CTL_ENGINE_GROUP_GT) break;
        }
      }
    }
  }

  uint32_t frequencyCount = 0;
  if (impl_->pEnumFrequencyDomains(impl_->device, &frequencyCount, nullptr) ==
          CTL_RESULT_SUCCESS &&
      frequencyCount > 0) {
    std::vector<ctl_freq_handle_t> frequencies(frequencyCount);
    if (impl_->pEnumFrequencyDomains(impl_->device, &frequencyCount,
                                     frequencies.data()) == CTL_RESULT_SUCCESS) {
      for (ctl_freq_handle_t frequency : frequencies) {
        ctl_freq_properties_t properties = {};
        properties.Size = sizeof(properties);
        if (impl_->pFrequencyGetProperties(frequency, &properties) ==
                CTL_RESULT_SUCCESS &&
            properties.type == CTL_FREQ_DOMAIN_GPU) {
          impl_->frequency = frequency;
          break;
        }
      }
    }
  }

  uint32_t temperatureCount = 0;
  if (impl_->pEnumTemperatureSensors(impl_->device, &temperatureCount, nullptr) ==
          CTL_RESULT_SUCCESS &&
      temperatureCount > 0) {
    std::vector<ctl_temp_handle_t> temperatures(temperatureCount);
    if (impl_->pEnumTemperatureSensors(impl_->device, &temperatureCount,
                                       temperatures.data()) == CTL_RESULT_SUCCESS) {
      for (ctl_temp_handle_t temperature : temperatures) {
        ctl_temp_properties_t properties = {};
        properties.Size = sizeof(properties);
        if (impl_->pTemperatureGetProperties(temperature, &properties) ==
            CTL_RESULT_SUCCESS) {
          if (!impl_->temperature || properties.type == CTL_TEMP_SENSORS_GPU)
            impl_->temperature = temperature;
          if (properties.type == CTL_TEMP_SENSORS_GPU) break;
        }
      }
    }
  }

  if (impl_->engine) {
    impl_->previousEngineStats = {};
    impl_->previousEngineStats.Size = sizeof(impl_->previousEngineStats);
    impl_->hasPreviousEngineStats =
        impl_->pEngineGetActivity(impl_->engine, &impl_->previousEngineStats) ==
        CTL_RESULT_SUCCESS;
  }
  return true;
#else
  (void)d3dDevice;
  return false;
#endif
}

void IntelMetrics::shutdown() {
#if defined(GPUOVERLAY_HAS_IGCL) && defined(_WIN64)
  if (!impl_) return;
  if (impl_->api && impl_->pClose) impl_->pClose(impl_->api);
  impl_->api = nullptr;
  impl_->device = nullptr;
  impl_->engine = nullptr;
  impl_->frequency = nullptr;
  impl_->temperature = nullptr;
  impl_->hasPreviousEngineStats = false;
  if (impl_->module) FreeLibrary(impl_->module);
  impl_->module = nullptr;
#endif
}

void IntelMetrics::update(ID3D11Device*, GPUMetrics& out) {
  reset_vendor_metrics(out);
  out.providerName = name();
#if defined(GPUOVERLAY_HAS_IGCL) && defined(_WIN64)
  if (!impl_ || !impl_->api || !impl_->device) return;

  if (impl_->engine) {
    ctl_engine_stats_t current = {};
    current.Size = sizeof(current);
    if (impl_->pEngineGetActivity(impl_->engine, &current) == CTL_RESULT_SUCCESS) {
      if (impl_->hasPreviousEngineStats &&
          current.timestamp > impl_->previousEngineStats.timestamp &&
          current.activeTime >= impl_->previousEngineStats.activeTime) {
        const uint64_t activeDelta =
            current.activeTime - impl_->previousEngineStats.activeTime;
        const uint64_t timeDelta =
            current.timestamp - impl_->previousEngineStats.timestamp;
        const double utilization = (std::clamp)(
            static_cast<double>(activeDelta) / static_cast<double>(timeDelta) *
                100.0,
            0.0, 100.0);
        out.gpuUsagePercent = static_cast<int>(std::lround(utilization));
        out.gpuUsageValid = true;
      }
      impl_->previousEngineStats = current;
      impl_->hasPreviousEngineStats = true;
    }
  }

  if (impl_->frequency) {
    ctl_freq_state_t state = {};
    state.Size = sizeof(state);
    if (impl_->pFrequencyGetState(impl_->frequency, &state) ==
            CTL_RESULT_SUCCESS &&
        state.actual >= 0.0) {
      out.engineClockMHz = static_cast<int>(std::lround(state.actual));
      out.engineClockValid = true;
    }
  }

  if (impl_->temperature) {
    double temperature = 0.0;
    if (impl_->pTemperatureGetState(impl_->temperature, &temperature) ==
        CTL_RESULT_SUCCESS) {
      out.temperatureC = static_cast<int>(std::lround(temperature));
      out.temperatureValid = true;
    }
  }
#endif
}

}  // namespace gpuoverlay
