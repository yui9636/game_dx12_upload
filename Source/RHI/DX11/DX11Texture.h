#pragma once
#include "../ITexture.h"
#include <d3d11.h>
#include <wrl.h>

class DX11Texture : public ITexture {
public:
    // コンストラクタでデバイスを受け取り、テクスチャと必要なビューを生成する
    DX11Texture(ID3D11Device* device, uint32_t width, uint32_t height, TextureFormat format, TextureBindFlags bindFlags);
    ~DX11Texture() override = default;

    DX11Texture(ID3D11Device* device, IDXGISwapChain* swapchain);

    uint32_t GetWidth() const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }
    TextureFormat GetFormat() const override { return m_format; }

    // void* にキャストして返す（使う側で元の型にキャストして使います）
    ID3D11Texture2D* GetNativeResource() const { return m_texture.Get(); }
    ID3D11ShaderResourceView* GetNativeSRV() const { return m_srv.Get(); }
    ID3D11RenderTargetView* GetNativeRTV() const { return m_rtv.Get(); }
    ID3D11DepthStencilView* GetNativeDSV() const { return m_dsv.Get(); }

    ResourceState GetCurrentState() const override { return m_currentState; }
    void SetCurrentState(ResourceState state) override { m_currentState = state; }


    DX11Texture(ID3D11ShaderResourceView* srv);

    DX11Texture(ID3D11DepthStencilView* dsv, uint32_t width, uint32_t height);

    DX11Texture(ID3D11RenderTargetView* rtv, uint32_t width, uint32_t height);
private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    TextureFormat m_format = TextureFormat::Unknown;
    ResourceState m_currentState = ResourceState::Common;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_dsv;

    // DX11用のフォーマット変換ヘルパー
    DXGI_FORMAT GetDXGIFormat(TextureFormat format);
};