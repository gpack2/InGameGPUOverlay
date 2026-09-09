#include "hook/overlay_renderer.h"
#include "hook/amd_metrics.h"
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <utility>

#pragma comment(lib, "d3dcompiler.lib")

namespace gpuoverlay {

namespace {

constexpr int OVERLAY_WIDTH = 320;
constexpr int OVERLAY_HEIGHT = 140;
constexpr int PADDING = 8;
constexpr int LINE_HEIGHT = 22;

static const char* vsSrc = R"(
struct VS_IN { float2 pos : POSITION; float2 uv : TEXCOORD0; };
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VS_OUT main(VS_IN i) {
  VS_OUT o;
  o.pos = float4(i.pos, 0, 1);
  o.uv = i.uv;
  return o;
}
)";

static const char* psSrc = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PS_IN i) : SV_Target {
  float4 c = tex.Sample(samp, i.uv);
  return float4(c.rgb, 0.78);
}
)";

template <typename T>
void release_if_set(T*& value) {
  if (value) {
    value->Release();
    value = nullptr;
  }
}

bool get_swap_chain_size(IDXGISwapChain* swapChain, int& width, int& height) {
  ID3D11Texture2D* backBuffer = nullptr;
  if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                  reinterpret_cast<void**>(&backBuffer))))
    return false;

  D3D11_TEXTURE2D_DESC desc = {};
  backBuffer->GetDesc(&desc);
  backBuffer->Release();
  width = static_cast<int>(desc.Width);
  height = static_cast<int>(desc.Height);
  return width > 0 && height > 0;
}

// Present hooks share the immediate context with the game. Restore every state
// slot touched by the overlay so rendering after Present is not corrupted.
class D3D11StateBackup {
public:
  explicit D3D11StateBackup(ID3D11DeviceContext* context) : context_(context) {
    context_->IAGetInputLayout(&inputLayout_);
    context_->IAGetVertexBuffers(0, 1, &vertexBuffer_, &vertexStride_, &vertexOffset_);
    context_->IAGetPrimitiveTopology(&topology_);
    context_->VSGetShader(&vertexShader_, nullptr, nullptr);
    context_->PSGetShader(&pixelShader_, nullptr, nullptr);
    context_->PSGetShaderResources(0, 1, &shaderResource_);
    context_->PSGetSamplers(0, 1, &sampler_);
    context_->OMGetBlendState(&blendState_, blendFactor_, &sampleMask_);
    context_->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                 renderTargets_, &depthStencilView_);
    context_->RSGetState(&rasterizerState_);
    viewportCount_ = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    context_->RSGetViewports(&viewportCount_, viewports_);
  }

  ~D3D11StateBackup() {
    context_->IASetInputLayout(inputLayout_);
    context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &vertexStride_, &vertexOffset_);
    context_->IASetPrimitiveTopology(topology_);
    context_->VSSetShader(vertexShader_, nullptr, 0);
    context_->PSSetShader(pixelShader_, nullptr, 0);
    context_->PSSetShaderResources(0, 1, &shaderResource_);
    context_->PSSetSamplers(0, 1, &sampler_);
    context_->OMSetBlendState(blendState_, blendFactor_, sampleMask_);
    context_->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                                 renderTargets_, depthStencilView_);
    context_->RSSetState(rasterizerState_);
    context_->RSSetViewports(viewportCount_, viewports_);

    release_if_set(inputLayout_);
    release_if_set(vertexBuffer_);
    release_if_set(vertexShader_);
    release_if_set(pixelShader_);
    release_if_set(shaderResource_);
    release_if_set(sampler_);
    release_if_set(blendState_);
    for (auto& target : renderTargets_) release_if_set(target);
    release_if_set(depthStencilView_);
    release_if_set(rasterizerState_);
  }

  D3D11StateBackup(const D3D11StateBackup&) = delete;
  D3D11StateBackup& operator=(const D3D11StateBackup&) = delete;

private:
  ID3D11DeviceContext* context_ = nullptr;
  ID3D11InputLayout* inputLayout_ = nullptr;
  ID3D11Buffer* vertexBuffer_ = nullptr;
  UINT vertexStride_ = 0;
  UINT vertexOffset_ = 0;
  D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  ID3D11VertexShader* vertexShader_ = nullptr;
  ID3D11PixelShader* pixelShader_ = nullptr;
  ID3D11ShaderResourceView* shaderResource_ = nullptr;
  ID3D11SamplerState* sampler_ = nullptr;
  ID3D11BlendState* blendState_ = nullptr;
  FLOAT blendFactor_[4] = {};
  UINT sampleMask_ = 0;
  ID3D11RenderTargetView* renderTargets_[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
  ID3D11DepthStencilView* depthStencilView_ = nullptr;
  ID3D11RasterizerState* rasterizerState_ = nullptr;
  UINT viewportCount_ = 0;
  D3D11_VIEWPORT viewports_[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
};

}  // namespace

OverlayRenderer::OverlayRenderer() = default;

OverlayRenderer::~OverlayRenderer() {
  shutdown();
}

bool OverlayRenderer::createResources(ID3D11Device* device, IDXGISwapChain* swapChain) {
  if (!get_swap_chain_size(swapChain, lastWidth_, lastHeight_)) return false;

  D3D11_TEXTURE2D_DESC texDesc = {};
  texDesc.Width = OVERLAY_WIDTH;
  texDesc.Height = OVERLAY_HEIGHT;
  texDesc.MipLevels = 1;
  texDesc.ArraySize = 1;
  texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.SampleDesc.Quality = 0;
  texDesc.Usage = D3D11_USAGE_DEFAULT;
  texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  texDesc.CPUAccessFlags = 0;
  texDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

  HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &textTexture_);
  if (FAILED(hr) || !textTexture_) return false;

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = texDesc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  hr = device->CreateShaderResourceView(textTexture_, &srvDesc, &textSrv_);
  if (FAILED(hr)) return false;

  ID3DBlob* vsBlob = nullptr;
  ID3DBlob* errBlob = nullptr;
  hr = D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errBlob);
  release_if_set(errBlob);
  if (FAILED(hr)) return false;
  hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_);
  if (FAILED(hr)) { vsBlob->Release(); return false; }

  D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };
  hr = device->CreateInputLayout(layoutDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout_);
  vsBlob->Release();
  if (FAILED(hr)) return false;

  ID3DBlob* psBlob = nullptr;
  hr = D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errBlob);
  release_if_set(errBlob);
  if (FAILED(hr)) return false;
  hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps_);
  psBlob->Release();
  if (FAILED(hr)) return false;

  D3D11_SAMPLER_DESC sampDesc = {};
  sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  hr = device->CreateSamplerState(&sampDesc, &sampler_);
  if (FAILED(hr)) return false;

  D3D11_BLEND_DESC blendDesc = {};
  blendDesc.RenderTarget[0].BlendEnable = TRUE;
  blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  hr = device->CreateBlendState(&blendDesc, &blendState_);
  if (FAILED(hr)) return false;

  D3D11_RASTERIZER_DESC rastDesc = {};
  rastDesc.FillMode = D3D11_FILL_SOLID;
  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.ScissorEnable = FALSE;
  rastDesc.DepthClipEnable = FALSE;
  hr = device->CreateRasterizerState(&rastDesc, &rasterizer_);
  if (FAILED(hr)) return false;

  struct Vertex { float x, y, u, v; };
  Vertex quad[] = {
    { -1,  1, 0, 0 },
    {  1,  1, 1, 0 },
    { -1, -1, 0, 1 },
    {  1, -1, 1, 1 },
  };
  D3D11_BUFFER_DESC vbDesc = {};
  vbDesc.ByteWidth = sizeof(quad);
  vbDesc.Usage = D3D11_USAGE_DEFAULT;
  vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vbData = { quad, 0, 0 };
  hr = device->CreateBuffer(&vbDesc, &vbData, &vertexBuffer_);
  if (FAILED(hr)) return false;

  font_ = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
  if (!font_) return false;

  return true;
}

bool OverlayRenderer::init(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain) {
  if (!device || !context || !swapChain) return false;
  shutdown();
  device_ = device;
  context_ = context;
  device_->AddRef();
  context_->AddRef();
  if (createResources(device, swapChain)) return true;
  shutdown();
  return false;
}

void OverlayRenderer::shutdown() {
  lastText_.clear();
  if (font_) { DeleteObject(font_); font_ = nullptr; }
  if (vertexBuffer_) { vertexBuffer_->Release(); vertexBuffer_ = nullptr; }
  if (rasterizer_) { rasterizer_->Release(); rasterizer_ = nullptr; }
  if (blendState_) { blendState_->Release(); blendState_ = nullptr; }
  if (sampler_) { sampler_->Release(); sampler_ = nullptr; }
  if (ps_) { ps_->Release(); ps_ = nullptr; }
  if (layout_) { layout_->Release(); layout_ = nullptr; }
  if (vs_) { vs_->Release(); vs_ = nullptr; }
  if (textSrv_) { textSrv_->Release(); textSrv_ = nullptr; }
  if (textTexture_) { textTexture_->Release(); textTexture_ = nullptr; }
  if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
  if (context_) { context_->Release(); context_ = nullptr; }
  if (device_) { device_->Release(); device_ = nullptr; }
}

void OverlayRenderer::drawTextQuad(ID3D11DeviceContext* context, int x, int y, int w, int h) {
  if (!vs_ || !ps_ || !layout_ || !textSrv_ || !sampler_ || !vertexBuffer_ || !blendState_ || !rasterizer_) return;

  float l = (x * 2.0f / lastWidth_) - 1.0f;
  float t = 1.0f - (y * 2.0f / lastHeight_);
  float r = ((x + w) * 2.0f / lastWidth_) - 1.0f;
  float b = 1.0f - ((y + h) * 2.0f / lastHeight_);

  struct Vertex { float x, y, u, v; };
  Vertex quad[] = {
    { l, t, 0, 0 },
    { r, t, 1, 0 },
    { l, b, 0, 1 },
    { r, b, 1, 1 },
  };
  context->UpdateSubresource(vertexBuffer_, 0, nullptr, quad, 0, 0);

  UINT stride = 16, offset = 0;
  context->IASetInputLayout(layout_);
  context->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  context->VSSetShader(vs_, nullptr, 0);
  context->PSSetShader(ps_, nullptr, 0);
  context->PSSetShaderResources(0, 1, &textSrv_);
  context->PSSetSamplers(0, 1, &sampler_);
  context->OMSetBlendState(blendState_, nullptr, 0xffffffff);
  context->RSSetState(rasterizer_);
  context->Draw(4, 0);
}

void OverlayRenderer::render(IDXGISwapChain* swapChain, ID3D11DeviceContext* context,
                             const GPUMetrics& metrics, int fps) {
  if (!swapChain || !context || !device_ || !textTexture_) return;

  int width = 0;
  int height = 0;
  if (!get_swap_chain_size(swapChain, width, height)) return;
  if (width != lastWidth_ || height != lastHeight_) {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
    lastWidth_ = width;
    lastHeight_ = height;
  }

  std::wostringstream ss;
  if (metrics.gpuUsageValid)
    ss << L"GPU: " << metrics.gpuUsagePercent << L"%\n";
  else
    ss << L"GPU: N/A\n";
  ss << L"VRAM: " << std::fixed << std::setprecision(2) << metrics.vramUsageGB << L" GB\n";
  ss << L"FPS: " << fps << L"\n";
  if (metrics.engineClockValid)
    ss << L"Clock: " << metrics.engineClockMHz << L" MHz\n";
  else
    ss << L"Clock: N/A\n";
  if (metrics.temperatureValid)
    ss << L"Temp: " << metrics.temperatureC << L" C";
  else
    ss << L"Temp: N/A";

  std::wstring text = ss.str();
  if (text != lastText_) {
    IDXGISurface1* surface = nullptr;
    if (FAILED(textTexture_->QueryInterface(__uuidof(IDXGISurface1),
                                            reinterpret_cast<void**>(&surface))))
      return;

    HDC hdc = nullptr;
    if (FAILED(surface->GetDC(FALSE, &hdc))) {
      surface->Release();
      return;
    }

    RECT rect = { 0, 0, OVERLAY_WIDTH, OVERLAY_HEIGHT };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 255, 0));
    HGDIOBJ oldFont = SelectObject(hdc, font_);

    int y = PADDING;
    for (size_t i = 0, start = 0; i <= text.size(); ++i) {
      if (i == text.size() || text[i] == L'\n') {
        std::wstring line(text.begin() + start, text.begin() + i);
        if (!line.empty())
          TextOutW(hdc, PADDING, y, line.c_str(), static_cast<int>(line.size()));
        y += LINE_HEIGHT;
        start = i + 1;
      }
    }

    SelectObject(hdc, oldFont);
    surface->ReleaseDC(nullptr);
    surface->Release();
    lastText_ = std::move(text);
  }

  ID3D11RenderTargetView* backBufferRtv = nullptr;
  ID3D11Texture2D* backBuffer = nullptr;
  if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))))
    return;
  if (FAILED(device_->CreateRenderTargetView(backBuffer, nullptr, &backBufferRtv))) {
    backBuffer->Release();
    return;
  }
  backBuffer->Release();

  D3D11StateBackup stateBackup(context);

  D3D11_VIEWPORT vp = {};
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  vp.Width = static_cast<float>(width);
  vp.Height = static_cast<float>(height);
  vp.MinDepth = 0;
  vp.MaxDepth = 1;
  context->RSSetViewports(1, &vp);
  context->OMSetRenderTargets(1, &backBufferRtv, nullptr);

  drawTextQuad(context, PADDING, PADDING, OVERLAY_WIDTH, OVERLAY_HEIGHT);

  backBufferRtv->Release();
}

}  // namespace gpuoverlay
