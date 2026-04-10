#pragma once
#include <d3d11.h>
#include <directxmath.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <d2d1_1.h>
#include <dwrite.h>

class D3D11Renderer {
public:
    struct Vertex {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 tex;
    };

    struct ConstantBuffer {
        DirectX::XMMATRIX world;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;
        DirectX::XMFLOAT4 color;
    };

    D3D11Renderer();
    ~D3D11Renderer();

    bool Initialize(HWND hWnd, int width, int height);
    void BeginRender(const DirectX::XMFLOAT4& clearColor);
    void EndRender();
    void Cleanup();

    void DrawTexture(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, float angle = 0.0f, float alpha = 1.0f, DirectX::XMFLOAT4 tint = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    void DrawTextW(const std::wstring& text, float x, float y, float size, DirectX::XMFLOAT4 color, bool centered = false);
    void DrawRect(float x, float y, float w, float h, DirectX::XMFLOAT4 color, bool fill = true);

    ID3D11Device* GetDevice() { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() { return m_context.Get(); }

    ID3D11ShaderResourceView* CreateTextureFromFile(const std::wstring& path);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerLinear;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;

    // D2D/DirectWrite for UI
    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> m_d2dRT;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatCenter;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;

    int m_screenWidth;
    int m_screenHeight;

    bool InitShaders();
};
