#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <dxgiformat.h>
#include "RHI/ITexture.h"

struct ID3D11Device;
struct IDXGISwapChain;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
class ICommandList;
class IResourceFactory;

class FrameBuffer
{
public:
    // DX11 swapchain back buffer を FrameBuffer として包む。
    FrameBuffer(ID3D11Device* device, IDXGISwapChain* swapchain);
    // DX11 の単一 HDR color + depth FrameBuffer を作る。
    FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height);
    // DX11 の複数 color attachment + depth FrameBuffer を作る。
    FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height,
        const std::vector<DXGI_FORMAT>& formats,
        TextureFormat depthFormat = TextureFormat::D32_FLOAT);

    // RHI factory 経由で DX11/DX12 共通の FrameBuffer texture 群を作る。
    FrameBuffer(IResourceFactory* factory, uint32_t width, uint32_t height,
        const std::vector<TextureFormat>& colorFormats,
        TextureFormat depthFormat = TextureFormat::D32_FLOAT,
        const float* optimizedClearColor = nullptr);

    ~FrameBuffer();

    // 既存の DX11 ネイティブ SRV/RTV 取得 API。DX12 では nullptr を返す。
    ID3D11ShaderResourceView* GetColorMap(size_t index = 0) const;
    ID3D11ShaderResourceView* GetDepthMap() const;
    ID3D11RenderTargetView* GetRenderTargetView(size_t index = 0) const;
    ID3D11DepthStencilView* GetDepthStencilView() const;

    // ImGui::Image へ直接渡せる API 共通のテクスチャ ID を返す。
    void* GetImGuiTextureID(size_t index = 0) const;

    ITexture* GetColorTexture(size_t index = 0) const {
        if (index < m_colorTextures.size()) return m_colorTextures[index].get();
        return nullptr;
    }

    ITexture* GetDepthTexture() const {
        return m_depthTexture.get();
    }

    // 全 color attachment と内部 depth を command list へ設定する。
    void SetRenderTargets(ICommandList* commandList);
    // color attachment と外部指定 depth を command list へ設定する。
    void SetRenderTarget(ICommandList* commandList, ITexture* depthStencil);
    // color/depth を render target state へ遷移して clear する。
    void Clear(ICommandList* commandList, float r, float g, float b, float a);

    uint32_t GetWidth() const { return static_cast<uint32_t>(m_width); }
    uint32_t GetHeight() const { return static_cast<uint32_t>(m_height); }

private:
    std::vector<std::unique_ptr<ITexture>> m_colorTextures; // color attachment 群。
    std::unique_ptr<ITexture> m_depthTexture; // depth stencil attachment。
    float m_width = 0.0f;  // texture 幅。
    float m_height = 0.0f; // texture 高さ。
};
