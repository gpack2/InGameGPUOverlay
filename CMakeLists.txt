cmake_minimum_required(VERSION 3.16)
project(GPUOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Windows only
if(NOT WIN32)
  message(FATAL_ERROR "GPUOverlay is a Windows-only application (AMD Radeon + DX11).")
endif()

# Tray application (executable)
add_executable(GPUOverlay WIN32
  src/tray/main.cpp
  src/tray/tray.cpp
  src/tray/injector.cpp
  src/tray/process_list.cpp
)
target_include_directories(GPUOverlay PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(GPUOverlay PRIVATE
  shell32
  user32
  kernel32
  gdi32
)

# Hook DLL (injected into game process)
add_library(GPUOverlayHook SHARED
  src/hook/dllmain.cpp
  src/hook/dx11_hook.cpp
  src/hook/overlay_renderer.cpp
  src/hook/amd_metrics.cpp
)
target_include_directories(GPUOverlayHook PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(GPUOverlayHook PRIVATE
  d3d11
  dxgi
  d3dcompiler
  user32
  kernel32
)

# MinHook (fetch or use bundled)
include(FetchContent)
FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)
target_include_directories(GPUOverlayHook PRIVATE ${minhook_SOURCE_DIR}/include)
target_link_libraries(GPUOverlayHook PRIVATE minhook)

# Copy hook DLL next to exe for easy injection
add_custom_command(TARGET GPUOverlay POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:GPUOverlayHook> $<TARGET_FILE_DIR:GPUOverlay>
)
