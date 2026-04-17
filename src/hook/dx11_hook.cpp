#include "hook/dx11_hook.h"
#include "MinHook.h"
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <utility>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace gpuoverlay {

namespace {

typedef HRESULT(WINAPI* PresentFn)(IDXGISwapChain*, UINT, UINT);

PresentFn g_originalPresent = nullptr;
void* g_presentAddress = nullptr;
PresentCallback g_presentCallback;
std::mutex g_callbackMutex;
std::condition_variable g_callbackIdle;
unsigned int g_activePresentCalls = 0;

HRESULT WINAPI hooked_present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
  PresentCallback callback;
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    ++g_activePresentCalls;
    callback = g_presentCallback;
  }

  if (callback && pSwapChain) {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device),
                                        reinterpret_cast<void**>(&device))) && device) {
      device->GetImmediateContext(&context);
      if (context) {
        try {
          callback(pSwapChain, device, context);
        } catch (...) {
          // Never allow overlay code to unwind through the game's Present call.
        }
        context->Release();
      }
      device->Release();
    }
  }

  const HRESULT result = g_originalPresent(pSwapChain, SyncInterval, Flags);
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    if (--g_activePresentCalls == 0) g_callbackIdle.notify_all();
  }
  return result;
}

}  // namespace

// Get Present address by creating a dummy swap chain (so we hook even if game already created device).
static void* get_present_address() {
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"GPUOverlayDummy";
  if (!RegisterClassExW(&wc)) return nullptr;

  HWND hwnd = CreateWindowExW(0, L"GPUOverlayDummy", nullptr, WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd) {
    UnregisterClassW(L"GPUOverlayDummy", wc.hInstance);
    return nullptr;
  }

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 1;
  sd.BufferDesc.Width = 1;
  sd.BufferDesc.Height = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;

  IDXGISwapChain* sc = nullptr;
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  D3D_FEATURE_LEVEL fl = {};
  typedef HRESULT(WINAPI* CreateFn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
  HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
  if (!d3d11) {
    DestroyWindow(hwnd);
    UnregisterClassW(L"GPUOverlayDummy", wc.hInstance);
    return nullptr;
  }
  auto createFn = reinterpret_cast<CreateFn>(GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain"));
  if (!createFn || FAILED(createFn(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx))) {
    DestroyWindow(hwnd);
    UnregisterClassW(L"GPUOverlayDummy", wc.hInstance);
    return nullptr;
  }
  void** vtable = *reinterpret_cast<void***>(sc);
  void* presentAddr = vtable[8];
  ctx->Release();
  dev->Release();
  sc->Release();
  DestroyWindow(hwnd);
  UnregisterClassW(L"GPUOverlayDummy", wc.hInstance);
  return presentAddr;
}

bool install_dx11_hook() {
  if (MH_Initialize() != MH_OK) return false;

  void* presentAddr = get_present_address();
  if (!presentAddr) {
    MH_Uninitialize();
    return false;
  }

  if (MH_CreateHook(presentAddr, &hooked_present, reinterpret_cast<LPVOID*>(&g_originalPresent)) != MH_OK) {
    MH_Uninitialize();
    return false;
  }
  if (MH_EnableHook(presentAddr) != MH_OK) {
    MH_RemoveHook(presentAddr);
    MH_Uninitialize();
    return false;
  }
  g_presentAddress = presentAddr;
  return true;
}

void remove_dx11_hook() {
  if (!g_presentAddress) return;
  MH_DisableHook(g_presentAddress);
  {
    std::unique_lock<std::mutex> lock(g_callbackMutex);
    g_presentCallback = nullptr;
    g_callbackIdle.wait(lock, [] { return g_activePresentCalls == 0; });
  }
  MH_RemoveHook(g_presentAddress);
  MH_Uninitialize();
  g_presentAddress = nullptr;
  g_originalPresent = nullptr;
}

void set_present_callback(PresentCallback cb) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_presentCallback = std::move(cb);
}

}  // namespace gpuoverlay
