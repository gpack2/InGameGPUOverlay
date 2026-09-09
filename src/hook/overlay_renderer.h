#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <string>

namespace gpuoverlay {

struct GPUMetrics;

class OverlayRenderer {
public:
  OverlayRenderer();
  ~OverlayRenderer();

  bool init(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain);
  void shutdown();

  void render(IDXGISwapChain* swapChain, ID3D11DeviceContext* context,
              const GPUMetrics& metrics, int fps);

private:
  bool createResources(ID3D11Device* device, IDXGISwapChain* swapChain);
  void drawTextQuad(ID3D11DeviceContext* context, int x, int y, int w, int h);

  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;
  ID3D11RenderTargetView* rtv_ = nullptr;
  ID3D11Texture2D* textTexture_ = nullptr;
  ID3D11ShaderResourceView* textSrv_ = nullptr;
  ID3D11VertexShader* vs_ = nullptr;
  ID3D11PixelShader* ps_ = nullptr;
  ID3D11InputLayout* layout_ = nullptr;
  ID3D11SamplerState* sampler_ = nullptr;
  ID3D11BlendState* blendState_ = nullptr;
  ID3D11RasterizerState* rasterizer_ = nullptr;
  ID3D11Buffer* vertexBuffer_ = nullptr;
  HFONT font_ = nullptr;
  std::wstring lastText_;

  int lastWidth_ = 0;
  int lastHeight_ = 0;
};

}  // namespace gpuoverlay
