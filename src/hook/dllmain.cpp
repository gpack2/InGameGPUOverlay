#include "hook/dx11_hook.h"
#include "hook/overlay_renderer.h"
#include "hook/telemetry_provider.h"
#include "common/overlay_config.h"
#include "common/diagnostics.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <memory>

namespace {

std::atomic<bool> g_running{true};
std::atomic<int> g_fps{0};
std::chrono::steady_clock::time_point g_fpsTime;
int g_fpsFrames = 0;

bool config_write_time(const std::wstring& path, FILETIME& writeTime) {
  WIN32_FILE_ATTRIBUTE_DATA data = {};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
    return false;
  writeTime = data.ftLastWriteTime;
  return true;
}

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
  gpuoverlay::initialize_diagnostics(L"hook");
  gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                          L"GPU Overlay hook starting");
  // Injection owns the overlay for the lifetime of the game. Pinning prevents an
  // external FreeLibrary call from unloading code while Present is executing it.
  HMODULE pinnedModule = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_PIN,
                     reinterpret_cast<LPCWSTR>(&g_running), &pinnedModule);

  const std::wstring configPath = gpuoverlay::default_config_path(pinnedModule);
  gpuoverlay::ensure_overlay_config(configPath);
  gpuoverlay::OverlayConfig config = gpuoverlay::load_overlay_config(configPath);
  gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                          L"Configuration loaded from " + configPath);
  FILETIME configWriteTime = {};
  bool hasConfigWriteTime = config_write_time(configPath, configWriteTime);

  g_fpsTime = std::chrono::steady_clock::now();

  gpuoverlay::OverlayRenderer overlay;
  std::unique_ptr<gpuoverlay::TelemetryProvider> telemetry;
  bool overlayInited = false;
  ID3D11Device* activeDevice = nullptr;
  gpuoverlay::GPUMetrics cachedMetrics;
  auto nextMetricsUpdate = std::chrono::steady_clock::time_point::min();
  auto nextConfigUpdate = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
  gpuoverlay::HotkeyState toggleHotkeyState;
  gpuoverlay::HotkeyState positionHotkeyState;
  bool overlayVisible = true;
  bool vendorMetricsWarningLogged = false;
  bool vramWarningLogged = false;
  std::mutex renderMutex;

  gpuoverlay::set_present_callback([&](IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!swapChain || !device || !context) return;
    std::unique_lock<std::mutex> lock(renderMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;

    if (!overlayInited || activeDevice != device) {
      overlay.shutdown();
      if (telemetry) telemetry->shutdown();
      telemetry = gpuoverlay::create_telemetry_provider(device);
      overlayInited = overlay.init(device, context, swapChain);
      activeDevice = overlayInited ? device : nullptr;
      vendorMetricsWarningLogged = false;
      vramWarningLogged = false;
      gpuoverlay::log_message(
          overlayInited ? gpuoverlay::LogLevel::Info : gpuoverlay::LogLevel::Error,
          overlayInited ? L"Overlay renderer initialized"
                        : L"Overlay renderer initialization failed");
    }
    if (!overlayInited) return;

    const auto now = std::chrono::steady_clock::now();
    if (now >= nextConfigUpdate) {
      FILETIME currentWriteTime = {};
      if (config_write_time(configPath, currentWriteTime) &&
          (!hasConfigWriteTime ||
           CompareFileTime(&currentWriteTime, &configWriteTime) != 0)) {
        config = gpuoverlay::load_overlay_config(configPath);
        configWriteTime = currentWriteTime;
        hasConfigWriteTime = true;
        gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                                L"Configuration reloaded");
      }
      nextConfigUpdate = now + std::chrono::seconds(1);
    }
    if (gpuoverlay::hotkey_pressed(config.toggleHotkey, toggleHotkeyState)) {
      overlayVisible = !overlayVisible;
      gpuoverlay::log_message(
          gpuoverlay::LogLevel::Info,
          overlayVisible ? L"Overlay shown by hotkey" : L"Overlay hidden by hotkey");
    }
    if (gpuoverlay::hotkey_pressed(config.cyclePositionHotkey,
                                   positionHotkeyState)) {
      config.position = gpuoverlay::next_position(config.position);
      gpuoverlay::save_overlay_config(configPath, config);
      gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                              L"Overlay position changed by hotkey");
    }

    fps_tick();
    if (!overlayVisible) return;

    if (now >= nextMetricsUpdate) {
      if (telemetry) telemetry->update(device, cachedMetrics);
      gpuoverlay::update_dxgi_memory(device, cachedMetrics);
      if (!vendorMetricsWarningLogged && telemetry &&
          std::wstring(telemetry->name()) != L"DXGI" &&
          !cachedMetrics.gpuUsageValid && !cachedMetrics.engineClockValid &&
          !cachedMetrics.temperatureValid) {
        gpuoverlay::log_message(gpuoverlay::LogLevel::Warning,
                                L"Vendor telemetry returned no metrics");
        vendorMetricsWarningLogged = true;
      }
      if (!vramWarningLogged && !cachedMetrics.vramUsageValid) {
        gpuoverlay::log_message(gpuoverlay::LogLevel::Warning,
                                L"DXGI VRAM telemetry is unavailable");
        vramWarningLogged = true;
      }
      nextMetricsUpdate = now + std::chrono::milliseconds(250);
    }

    overlay.render(swapChain, context, cachedMetrics, g_fps.load(), config);
  });

  if (!gpuoverlay::install_dx11_hook()) {
    gpuoverlay::set_present_callback(nullptr);
    gpuoverlay::log_message(gpuoverlay::LogLevel::Error,
                            L"GPU Overlay hook startup failed");
    return 0;
  }

  while (g_running) {
    Sleep(100);
  }

  gpuoverlay::remove_dx11_hook();
  gpuoverlay::set_present_callback(nullptr);
  overlay.shutdown();
  if (telemetry) telemetry->shutdown();
  gpuoverlay::log_message(gpuoverlay::LogLevel::Info,
                          L"GPU Overlay hook stopped");
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
  (void)lpReserved;
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
