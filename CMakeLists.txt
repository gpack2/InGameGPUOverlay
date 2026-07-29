cmake_minimum_required(VERSION 3.16)
project(GPUOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows only
if(NOT WIN32)
  message(FATAL_ERROR "GPUOverlay is a Windows-only DirectX 11 application.")
endif()

# Tray application (executable)
add_executable(GPUOverlay WIN32
  src/common/overlay_config.cpp
  src/tray/main.cpp
  src/tray/tray.cpp
  src/tray/settings_dialog.cpp
  src/tray/injector.cpp
  src/tray/process_list.cpp
)
target_include_directories(GPUOverlay PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_definitions(GPUOverlay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(GPUOverlay PRIVATE
  shell32
  user32
  kernel32
  gdi32
)

# Hook DLL (injected into game process)
add_library(GPUOverlayHook SHARED
  src/common/overlay_config.cpp
  src/hook/dllmain.cpp
  src/hook/dx11_hook.cpp
  src/hook/overlay_renderer.cpp
  src/hook/telemetry_provider.cpp
  src/hook/amd_metrics.cpp
  src/hook/nvidia_metrics.cpp
  src/hook/intel_metrics.cpp
)
target_include_directories(GPUOverlayHook PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_definitions(GPUOverlayHook PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(GPUOverlayHook PRIVATE
  d3d11
  dxgi
  d3dcompiler
  gdi32
  user32
  kernel32
)

# MinHook (fetch or use bundled)
include(FetchContent)
FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG v1.3.4
)
FetchContent_MakeAvailable(minhook)
target_include_directories(GPUOverlayHook PRIVATE ${minhook_SOURCE_DIR}/include)
target_link_libraries(GPUOverlayHook PRIVATE minhook)

# NVIDIA NVAPI SDK (the implementation is supplied by the installed driver)
FetchContent_Declare(
  nvapi
  GIT_REPOSITORY https://github.com/NVIDIA/nvapi.git
  GIT_TAG 87dca625e83fd89a983e19b904e5f3a580da90d2
)
FetchContent_MakeAvailable(nvapi)
add_library(nvapi_sdk UNKNOWN IMPORTED)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set_target_properties(nvapi_sdk PROPERTIES
    IMPORTED_LOCATION "${nvapi_SOURCE_DIR}/amd64/nvapi64.lib"
  )
else()
  set_target_properties(nvapi_sdk PROPERTIES
    IMPORTED_LOCATION "${nvapi_SOURCE_DIR}/x86/nvapi.lib"
  )
endif()
target_include_directories(GPUOverlayHook PRIVATE ${nvapi_SOURCE_DIR})
target_link_libraries(GPUOverlayHook PRIVATE nvapi_sdk)

# Intel Graphics Control Library headers. Telemetry APIs are x64-only; the
# provider loads ControlLib.dll from the installed Intel driver at runtime.
FetchContent_Declare(
  igcl
  GIT_REPOSITORY https://github.com/intel/drivers.gpu.control-library.git
  GIT_TAG b6c462933502e13d1537dd5024949a51be30e63d
)
FetchContent_MakeAvailable(igcl)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  target_include_directories(GPUOverlayHook PRIVATE ${igcl_SOURCE_DIR}/include)
  target_compile_definitions(GPUOverlayHook PRIVATE GPUOVERLAY_HAS_IGCL=1)
endif()

if(MSVC)
  target_compile_options(GPUOverlay PRIVATE /W4 /permissive-)
  target_compile_options(GPUOverlayHook PRIVATE /W4 /permissive-)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(GPUOverlayConfigTests
    tests/config_tests.cpp
    src/common/overlay_config.cpp
  )
  target_include_directories(GPUOverlayConfigTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_definitions(GPUOverlayConfigTests PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
  target_link_libraries(GPUOverlayConfigTests PRIVATE user32 kernel32)
  if(MSVC)
    target_compile_options(GPUOverlayConfigTests PRIVATE /W4 /permissive-)
  endif()
  add_test(NAME config COMMAND GPUOverlayConfigTests)
endif()

# Copy hook DLL next to exe for easy injection
add_dependencies(GPUOverlay GPUOverlayHook)
add_custom_command(TARGET GPUOverlay POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:GPUOverlayHook> $<TARGET_FILE_DIR:GPUOverlay>
)
