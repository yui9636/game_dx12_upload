#include "Render/FrameBuffer.h"
#include "System/Misc.h"
#include "RHI/ICommandList.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/IResourceFactory.h"
#include "Render/ImGuiRenderer.h"


static TextureFormat ConvertDXGIToTextureFormat(DXGI_FORMAT format) {
    // DX11 旧 API の DXGI_FORMAT を RHI の TextureFormat へ寄せる。
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:     return TextureFormat::RGBA8_UNORM;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return TextureFormat::R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32G32B32_FLOAT:    return TextureFormat::R32G32B32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return TextureFormat::R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32_FLOAT:       return TextureFormat::R32G32_FLOAT;
    case DXGI_FORMAT_R16G16_FLOAT:       return TextureFormat::R16G16_FLOAT;
    case DXGI_FORMAT_R8_UNORM:           return TextureFormat::R8_UNORM;
    default:                             return TextureFormat::Unknown;
    }
}

FrameBuffer::~FrameBuffer() = default;

FrameBuffer::FrameBuffer(IResourceFactory* factory, uint32_t width, uint32_t height,
    const std::vector<TextureFormat>& colorFormats, TextureFormat depthFormat,
    const float* optimizedClearColor)
    : m_width((float)width), m_height((float)height)
{
    // RHI path。DX11/DX12 の違いは factory 側へ隠し、FrameBuffer は ITexture だけを持つ。
    TextureDesc desc;
    desc.width = width;
    desc.height = height;
    if (optimizedClearColor) {
        desc.clearColor[0] = optimizedClearColor[0];
        desc.clearColor[1] = optimizedClearColor[1];
        desc.clearColor[2] = optimizedClearColor[2];
        desc.clearColor[3] = optimizedClearColor[3];
    }
    for (TextureFormat fmt : colorFormats) {
        desc.format = fmt;
        // ビュー履歴とプレビュー用フレームバッファはサンプリングと描画に使うが、UAV としては使わない。
        // ここで UAV を要求すると形式とフラグの条件が広がり、PlayerEditor の DX12 リソース生成で失敗していた。
        desc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
        m_colorTextures.push_back(factory->CreateTexture("fb_color", desc));
    }
    desc.format = depthFormat;
    desc.bindFlags = TextureBindFlags::DepthStencil | TextureBindFlags::ShaderResource;
    m_depthTexture = factory->CreateTexture("fb_depth", desc);
}

FrameBuffer::FrameBuffer(ID3D11Device* device, IDXGISwapChain* swapchain)
{
    // swapchain back buffer を color attachment として包む DX11 専用 path。
    auto tex = std::make_unique<DX11Texture>(device, swapchain);
    m_width = (float)tex->GetWidth();
    m_height = (float)tex->GetHeight();

    m_depthTexture = std::make_unique<DX11Texture>(device, (uint32_t)m_width, (uint32_t)m_height,
        TextureFormat::D32_FLOAT, TextureBindFlags::DepthStencil | TextureBindFlags::ShaderResource);

    m_colorTextures.push_back(std::move(tex));
}

FrameBuffer::FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height)
    : FrameBuffer(device, width, height, std::vector<DXGI_FORMAT>{ DXGI_FORMAT_R16G16B16A16_FLOAT }, TextureFormat::D32_FLOAT)
{
}

FrameBuffer::FrameBuffer(ID3D11Device* device, uint32_t width, uint32_t height,
    const std::vector<DXGI_FORMAT>& formats,
    TextureFormat depthFormat)
    : m_width((float)width), m_height((float)height)
{
    // DX11 path。旧 render pass が DXGI_FORMAT 指定で FrameBuffer を作るため変換して保持する。
    for (DXGI_FORMAT dxgiFmt : formats)
    {
        TextureFormat fmt = ConvertDXGIToTextureFormat(dxgiFmt);
        auto tex = std::make_unique<DX11Texture>(device, width, height, fmt,
            TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource | TextureBindFlags::UnorderedAccess);
        m_colorTextures.push_back(std::move(tex));
    }

    m_depthTexture = std::make_unique<DX11Texture>(device, width, height,
        depthFormat, TextureBindFlags::DepthStencil | TextureBindFlags::ShaderResource);
}


ID3D11ShaderResourceView* FrameBuffer::GetColorMap(size_t index) const {
    // DX12 texture では native DX11 SRV を持たないため nullptr になる。
    if (index >= m_colorTextures.size()) return nullptr;
    auto* dx11 = dynamic_cast<DX11Texture*>(m_colorTextures[index].get());
    return dx11 ? dx11->GetNativeSRV() : nullptr;
}

ID3D11ShaderResourceView* FrameBuffer::GetDepthMap() const {
    if (!m_depthTexture) return nullptr;
    auto* dx11 = dynamic_cast<DX11Texture*>(m_depthTexture.get());
    return dx11 ? dx11->GetNativeSRV() : nullptr;
}

ID3D11RenderTargetView* FrameBuffer::GetRenderTargetView(size_t index) const {
    if (index >= m_colorTextures.size()) return nullptr;
    auto* dx11 = dynamic_cast<DX11Texture*>(m_colorTextures[index].get());
    return dx11 ? dx11->GetNativeRTV() : nullptr;
}

ID3D11DepthStencilView* FrameBuffer::GetDepthStencilView() const {
    if (!m_depthTexture) return nullptr;
    auto* dx11 = dynamic_cast<DX11Texture*>(m_depthTexture.get());
    return dx11 ? dx11->GetNativeDSV() : nullptr;
}

void* FrameBuffer::GetImGuiTextureID(size_t index) const {
    // backend ごとの ImTextureID 変換は ImGuiRenderer に集約する。
    if (index >= m_colorTextures.size()) return nullptr;
    return ImGuiRenderer::GetTextureID(m_colorTextures[index].get());
}

void FrameBuffer::Clear(ICommandList* commandList, float r, float g, float b, float a)
{
    // clear 前に各 attachment を適切な write state へ遷移する。
    float color[4]{ r, g, b, a };
    for (const auto& tex : m_colorTextures) {
        commandList->TransitionBarrier(tex.get(), ResourceState::RenderTarget);
        commandList->ClearColor(tex.get(), color);
    }
    if (m_depthTexture) {
        commandList->TransitionBarrier(m_depthTexture.get(), ResourceState::DepthWrite);
        commandList->ClearDepthStencil(m_depthTexture.get(), 1.0f, 0);
    }
}

void FrameBuffer::SetRenderTargets(ICommandList* commandList)
{
    // FrameBuffer サイズに viewport を合わせ、内部 depth を使う。
    commandList->SetViewport(RhiViewport(0.0f, 0.0f, m_width, m_height));

    std::vector<ITexture*> rtvs;
    for (const auto& tex : m_colorTextures) rtvs.push_back(tex.get());

    commandList->SetRenderTargets((uint32_t)rtvs.size(), rtvs.data(), m_depthTexture.get());
}

void FrameBuffer::SetRenderTarget(ICommandList* commandList, ITexture* depthStencil)
{
    // shadow pass 後など、外部 depth を使う場合の render target 設定。
    commandList->SetViewport(RhiViewport(0.0f, 0.0f, m_width, m_height));

    std::vector<ITexture*> rtvs;
    for (const auto& tex : m_colorTextures) rtvs.push_back(tex.get());

    commandList->SetRenderTargets((uint32_t)rtvs.size(), rtvs.data(), depthStencil);
}
