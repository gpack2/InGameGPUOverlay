// Minimal ADL declarations for GPU metrics (AMD Radeon).
// Full SDK: https://gpuopen.com/adl/
// We load atiadlxx.dll at runtime; no SDK .lib required.

#pragma once

#ifdef _WIN32

#include <cstdint>

#define ADL_OK 0
#define ADL_MAX_PATH 256

typedef void* ADL_CONTEXT_HANDLE;
typedef void* ADL_MAIN_MALLOC_CALLBACK;

extern "C" {

struct ADLPMActivity {
  int iSize;
  int iActivityPercent;
  int iEngineClock;
  int iMemoryClock;
  int iVddc;
  int iCurrentBusLanes;
  int iCurrentBusSpeed;
  int iCurrentPerformanceLevel;
  int iMaximumBusLanes;
  int iReserved;
};

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

// ADL_Main_Memory_Alloc
typedef void* (*ADL_MAIN_MEMORY_ALLOC)(int iSize);

}  // extern "C"

#endif  // _WIN32
