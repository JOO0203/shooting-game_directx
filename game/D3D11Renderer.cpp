#include "D3D11Renderer.h"
#include <d3dcompiler.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

const char* shaderSource = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

cbuffer ConstantBuffer : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    float4 tint;
};

PS_INPUT vs_main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = mul(float4(input.pos, 1.0f), world);
    output.pos = mul(output.pos, view);
    output.pos = mul(output.pos, projection);
    output.tex = input.tex;
    return output;
}

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

float4 ps_main(PS_INPUT input) : SV_Target {
    return txDiffuse.Sample(samLinear, input.tex) * tint;
}
)";

D3D11Renderer::D3D11Renderer() : m_screenWidth(0), m_screenHeight(0) {}

D3D11Renderer::~D3D11Renderer() { Cleanup(); }

bool D3D11Renderer::Initialize(HWND hWnd, int width, int height) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    m_screenWidth = width;
    m_screenHeight = height;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
        featureLevels, 1, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_device, &featureLevel, &m_context);

    if (FAILED(hr)) return false;

    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    m_device->CreateRenderTargetView(backBuffer.Get(), NULL, &m_renderTargetView);

    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), NULL);

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

    if (!InitShaders()) return false;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    m_device->CreateSamplerState(&sampDesc, &m_samplerLinear);

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_device->CreateBlendState(&blendDesc, &m_blendState);

    // Initialize D2D/DWrite
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &m_d2dFactory);
    if (FAILED(hr)) return false;
    
    // Simpler D2D init
    ComPtr<IDXGISurface> surface;
    hr = m_swapChain->GetBuffer(0, __uuidof(IDXGISurface), &surface);
    if (FAILED(hr)) return false;
    
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
    hr = m_d2dFactory->CreateDxgiSurfaceRenderTarget(surface.Get(), &props, &m_d2dRT);
    if (FAILED(hr)) return false;
    
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &m_dwriteFactory);
    if (FAILED(hr)) return false;

    m_d2dRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_textBrush);

    hr = m_dwriteFactory->CreateTextFormat(L"Arial", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"ko-KR", &m_textFormat);
    if (FAILED(hr)) return false;

    hr = m_dwriteFactory->CreateTextFormat(L"Arial", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"ko-KR", &m_textFormatCenter);
    if (FAILED(hr)) return false;
    m_textFormatCenter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    return true;
}

bool D3D11Renderer::InitShaders() {
    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr = D3DCompile(shaderSource, strlen(shaderSource), NULL, NULL, NULL, "vs_main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        return false;
    }
    m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &m_vertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);

    hr = D3DCompile(shaderSource, strlen(shaderSource), NULL, NULL, NULL, "ps_main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        return false;
    }
    m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &m_pixelShader);

    D3D11Renderer::Vertex vertices[] = {
        { {-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f} },
        { {-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f} },
        { { 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f} },
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(D3D11Renderer::Vertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    m_device->CreateBuffer(&bd, &initData, &m_vertexBuffer);

    bd.ByteWidth = sizeof(D3D11Renderer::ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    m_device->CreateBuffer(&bd, NULL, &m_constantBuffer);

    return true;
}

void D3D11Renderer::BeginRender(const DirectX::XMFLOAT4& clearColor) {
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), (const float*)&clearColor);
    m_context->IASetInputLayout(m_inputLayout.Get());
    UINT stride = sizeof(D3D11Renderer::Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_context->VSSetShader(m_vertexShader.Get(), NULL, 0);
    m_context->PSSetShader(m_pixelShader.Get(), NULL, 0);
    m_context->PSSetSamplers(0, 1, m_samplerLinear.GetAddressOf());
    m_context->OMSetBlendState(m_blendState.Get(), NULL, 0xFFFFFFFF);
}

void D3D11Renderer::DrawTexture(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, float angle, float alpha, DirectX::XMFLOAT4 tint) {
    if (!srv) return;

    DirectX::XMMATRIX world = DirectX::XMMatrixScaling(w, h, 1.0f) *
        DirectX::XMMatrixRotationZ(angle) *
        DirectX::XMMatrixTranslation(x - m_screenWidth / 2.0f, m_screenHeight / 2.0f - y, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX projection = DirectX::XMMatrixOrthographicLH((float)m_screenWidth, (float)m_screenHeight, 0.0f, 100.0f);

    D3D11Renderer::ConstantBuffer cb;
    cb.world = DirectX::XMMatrixTranspose(world);
    cb.view = DirectX::XMMatrixTranspose(view);
    cb.projection = DirectX::XMMatrixTranspose(projection);
    cb.color = tint;
    cb.color.w *= alpha;

    m_context->UpdateSubresource(m_constantBuffer.Get(), 0, NULL, &cb, 0, 0);
    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetShaderResources(0, 1, &srv);
    m_context->Draw(4, 0);
}

void D3D11Renderer::EndRender() {
    m_swapChain->Present(0, 0);
}

void D3D11Renderer::DrawTextW(const std::wstring& text, float x, float y, float size, DirectX::XMFLOAT4 color, bool centered) {
    if (!m_d2dRT || !m_textBrush) return;
    
    m_d2dRT->BeginDraw();
    m_textBrush->SetColor(D2D1::ColorF(color.x, color.y, color.z, color.w));
    
    IDWriteTextFormat* fmt = centered ? m_textFormatCenter.Get() : m_textFormat.Get();
    if (!fmt) { m_d2dRT->EndDraw(); return; }

    D2D1_RECT_F rect = D2D1::RectF(x - (centered ? 500 : 0), y, x + (centered ? 500 : 1000), y + 100);
    m_d2dRT->DrawText(text.c_str(), (UINT32)text.length(), fmt, rect, m_textBrush.Get());
    m_d2dRT->EndDraw();
}

void D3D11Renderer::DrawRect(float x, float y, float w, float h, DirectX::XMFLOAT4 color, bool fill) {
    if (!m_d2dRT) return;
    m_d2dRT->BeginDraw();
    m_textBrush->SetColor(D2D1::ColorF(color.x, color.y, color.z, color.w));
    D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
    if (fill) m_d2dRT->FillRectangle(rect, m_textBrush.Get());
    else m_d2dRT->DrawRectangle(rect, m_textBrush.Get());
    m_d2dRT->EndDraw();
}

void D3D11Renderer::Cleanup() {}

ID3D11ShaderResourceView* D3D11Renderer::CreateTextureFromFile(const std::wstring& path) {
    static bool wicInit = false;
    if (!wicInit) { CoInitializeEx(NULL, COINIT_MULTITHREADED); wicInit = true; }

    ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) return nullptr;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;

    ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);

    UINT width, height;
    converter->GetSize(&width, &height);

    std::vector<unsigned char> buffer(width * height * 4);
    converter->CopyPixels(NULL, width * 4, (UINT)buffer.size(), buffer.data());

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = buffer.data();
    data.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(m_device->CreateTexture2D(&desc, &data, &tex))) return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    m_device->CreateShaderResourceView(tex.Get(), NULL, &srv);
    return srv;
}
