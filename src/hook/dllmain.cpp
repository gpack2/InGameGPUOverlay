#include "hook/dx11_hook.h"
#include "hook/overlay_renderer.h"
#include "hook/amd_metrics.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace {

std::atomic<bool> g_running{true};
std::atomic<int> g_fps{0};
std::chrono::steady_clock::time_point g_fpsTime;
int g_fpsFrames = 0;

void fps_tick() {
  auto now = std::chrono::steady_clock::now();
  g_fpsFrames++;
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_fpsTime).count();
  if (elapsed >= 1000) {
    g_fps = static_cast<int>(g_fpsFrames * 1000 / elapsed);
    g_fpsFrames = 0;
    g_fpsTime = now;
  }
}

void hook_thread() {
  g_fpsTime = std::chrono::steady_clock::now();

  gpuoverlay::AMDMetrics amd;
  if (!amd.init()) {
    return;  // No AMD GPU or driver
  }

  gpuoverlay::OverlayRenderer overlay;
  bool overlayInited = false;

  gpuoverlay::set_present_callback([&](IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!swapChain || !device || !context) return;

    if (!overlayInited) {
      overlayInited = overlay.init(device, context, swapChain);
    }
    if (!overlayInited) return;

    gpuoverlay::GPUMetrics metrics;
    amd.update(device, metrics);
    fps_tick();

    overlay.render(swapChain, context, metrics, g_fps.load());
  });

  if (!gpuoverlay::install_dx11_hook()) {
    amd.shutdown();
    return;
  }

  while (g_running) {
    Sleep(100);
  }

  gpuoverlay::remove_dx11_hook();
  gpuoverlay::set_present_callback(nullptr);
  overlay.shutdown();
  amd.shutdown();
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(hModule);
      std::thread(hook_thread).detach();
      break;
    case DLL_PROCESS_DETACH:
      g_running = false;
      Sleep(200);
      break;
  }
  return TRUE;
}
