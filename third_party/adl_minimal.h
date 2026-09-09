// Minimal ADL declarations for GPU metrics (AMD Radeon).
// Full SDK: https://gpuopen.com/adl/
// We load atiadlxx.dll at runtime; no SDK .lib required.

#pragma once

#ifdef _WIN32

#include <cstdint>
#include <cstddef>

#define ADL_OK 0
#define ADL_MAX_PATH 256

typedef void* ADL_CONTEXT_HANDLE;
typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int iSize);

extern "C" {

struct ADLPMActivity {
  int iSize;
  int iEngineClock;
  int iMemoryClock;
  int iVddc;
  int iActivityPercent;
  int iCurrentPerformanceLevel;
  int iCurrentBusSpeed;
  int iCurrentBusLanes;
  int iMaximumBusLanes;
  int iReserved;
};

static_assert(sizeof(ADLPMActivity) == 40, "ADLPMActivity ABI size mismatch");
static_assert(offsetof(ADLPMActivity, iActivityPercent) == 16,
              "ADLPMActivity ABI layout mismatch");

struct ADLTemperature {
  int iSize;
  int iTemperature;  // millidegrees Celsius
};

// ADL2_Main_Control_Create
typedef int (*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK callback, int iEnumConnectedAdapters, ADL_CONTEXT_HANDLE* context);

// ADL2_Main_Control_Destroy
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE context);

// ADL2_Overdrive5_CurrentActivity_Get
typedef int (*ADL2_OVERDRIVE5_CURRENTACTIVITY_GET)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADLPMActivity* lpActivity);

// ADL2_Overdrive5_Temperature_Get (iThermalControllerIndex 0 = GPU)
typedef int (*ADL2_OVERDRIVE5_TEMPERATURE_GET)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, int iThermalControllerIndex, ADLTemperature* lpTemperature);

}  // extern "C"

#endif  // _WIN32
