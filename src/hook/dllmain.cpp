#include "hook/dx11_hook.h"
#include "hook/overlay_renderer.h"
#include "hook/amd_metrics.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>

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

DWORD WINAPI hook_thread(LPVOID) {
  // Injection owns the overlay for the lifetime of the game. Pinning prevents an
  // external FreeLibrary call from unloading code while Present is executing it.
  HMODULE pinnedModule = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_PIN,
                     reinterpret_cast<LPCWSTR>(&g_running), &pinnedModule);

  g_fpsTime = std::chrono::steady_clock::now();

  gpuoverlay::AMDMetrics amd;
  // ADL is optional: non-AMD systems still get FPS and DXGI VRAM usage.
  amd.init();

  gpuoverlay::OverlayRenderer overlay;
  bool overlayInited = false;
  ID3D11Device* activeDevice = nullptr;
  gpuoverlay::GPUMetrics cachedMetrics;
  auto nextMetricsUpdate = std::chrono::steady_clock::time_point::min();
  std::mutex renderMutex;

  gpuoverlay::set_present_callback([&](IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!swapChain || !device || !context) return;
    std::unique_lock<std::mutex> lock(renderMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;

    if (!overlayInited || activeDevice != device) {
      overlay.shutdown();
      overlayInited = overlay.init(device, context, swapChain);
      activeDevice = overlayInited ? device : nullptr;
    }
    if (!overlayInited) return;

    const auto now = std::chrono::steady_clock::now();
    if (now >= nextMetricsUpdate) {
      amd.update(device, cachedMetrics);
      nextMetricsUpdate = now + std::chrono::milliseconds(250);
    }
    fps_tick();

    overlay.render(swapChain, context, cachedMetrics, g_fps.load());
  });

  if (!gpuoverlay::install_dx11_hook()) {
    gpuoverlay::set_present_callback(nullptr);
    amd.shutdown();
    return 0;
  }

  while (g_running) {
    Sleep(100);
  }

  gpuoverlay::remove_dx11_hook();
  gpuoverlay::set_present_callback(nullptr);
  overlay.shutdown();
  amd.shutdown();
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(hModule);
      if (HANDLE thread = CreateThread(nullptr, 0, hook_thread, nullptr, 0, nullptr))
        CloseHandle(thread);
      break;
    case DLL_PROCESS_DETACH:
      g_running = false;
      break;
  }
  return TRUE;
}
