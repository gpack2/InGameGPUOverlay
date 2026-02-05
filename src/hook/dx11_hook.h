#pragma once

#include <functional>
#include <d3d11.h>
#include <dxgi.h>

namespace gpuoverlay {

using PresentCallback = std::function<void(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context)>;

bool install_dx11_hook();
void remove_dx11_hook();
void set_present_callback(PresentCallback cb);

}  // namespace gpuoverlay
