//#include "System/Misc.h"
//#include "FrameBuffer.h"
//
//FrameBuffer::FrameBuffer(ID3D11Device* device, IDXGISwapChain* swapchain)
//{
//    HRESULT hr = S_OK;
//    UINT width, height;
//
//    {
//        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
//        hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(texture2d.GetAddressOf()));
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
//        hr = device->CreateRenderTargetView(texture2d.Get(), nullptr, rtv.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        renderTargetViews.push_back(rtv);
//
//        D3D11_TEXTURE2D_DESC texture2dDesc;
//        texture2d->GetDesc(&texture2dDesc);
//        width = texture2dDesc.Width;
//        height = texture2dDesc.Height;
//    }
//
//    {
//        viewport.Width = static_cast<float>(width);
//        viewport.Height = static_cast<float>(height);
//        viewport.MinDepth = 0.0f;
//        viewport.MaxDepth = 1.0f;
//        viewport.TopLeftX = 0.0f;
//        viewport.TopLeftY = 0.0f;
//    }
//
//    {
//        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
//        D3D11_TEXTURE2D_DESC texture2dDesc{};
//        texture2dDesc.Width = width;
//        texture2dDesc.Height = height;
//        texture2dDesc.MipLevels = 1;
//        texture2dDesc.ArraySize = 1;
//        texture2dDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
//        texture2dDesc.SampleDesc.Count = 1;
//        texture2dDesc.SampleDesc.Quality = 0;
//        texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
//        texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
//        hr = device->CreateTexture2D(&texture2dDesc, nullptr, texture2d.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        hr = device->CreateDepthStencilView(texture2d.Get(), nullptr, depthStencilView.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//    }
//}
//
//// ====================================================================
//// ====================================================================
//FrameBuffer::FrameBuffer(ID3D11Device* device, UINT width, UINT height)
//    : FrameBuffer(device, width, height, std::vector<DXGI_FORMAT>{ DXGI_FORMAT_R16G16B16A16_FLOAT })
//{
//}
//
//// ====================================================================
//// ====================================================================
//FrameBuffer::FrameBuffer(ID3D11Device* device, UINT width, UINT height, const std::vector<DXGI_FORMAT>& formats)
//{
//    HRESULT hr = S_OK;
//
//    for (DXGI_FORMAT format : formats)
//    {
//        Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTargetBuffer;
//        D3D11_TEXTURE2D_DESC texture2dDesc{};
//        texture2dDesc.Width = width;
//        texture2dDesc.Height = height;
//        texture2dDesc.MipLevels = 1;
//        texture2dDesc.ArraySize = 1;
//        texture2dDesc.SampleDesc.Count = 1;
//        texture2dDesc.SampleDesc.Quality = 0;
//        texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
//        texture2dDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
//        texture2dDesc.CPUAccessFlags = 0;
//        texture2dDesc.MiscFlags = 0;
//        hr = device->CreateTexture2D(&texture2dDesc, 0, renderTargetBuffer.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
//        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
//        rtvDesc.Format = format;
//        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
//        hr = device->CreateRenderTargetView(renderTargetBuffer.Get(), &rtvDesc, rtv.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//
//        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
//        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//        srvDesc.Format = format;
//        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
//        srvDesc.Texture2D.MipLevels = 1;
//        hr = device->CreateShaderResourceView(renderTargetBuffer.Get(), &srvDesc, srv.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//    }
//
//    {
//        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;
//        D3D11_TEXTURE2D_DESC texture2dDesc{};
//        texture2dDesc.Width = width;
//        texture2dDesc.Height = height;
//        texture2dDesc.MipLevels = 1;
//        texture2dDesc.ArraySize = 1;
//        texture2dDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
//        texture2dDesc.SampleDesc.Count = 1;
//        texture2dDesc.SampleDesc.Quality = 0;
//        texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
//        texture2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
//        hr = device->CreateTexture2D(&texture2dDesc, 0, depthStencilBuffer.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
//        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
//        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
//        hr = device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, depthStencilView.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//
//        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
//        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
//        srvDesc.Texture2D.MipLevels = 1;
//        hr = device->CreateShaderResourceView(depthStencilBuffer.Get(), &srvDesc, depthMap.GetAddressOf());
//        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
//    }
//
//    {
//        viewport.Width = static_cast<float>(width);
//        viewport.Height = static_cast<float>(height);
//        viewport.MinDepth = 0.0f;
//        viewport.MaxDepth = 1.0f;
//        viewport.TopLeftX = 0.0f;
//        viewport.TopLeftY = 0.0f;
//    }
//}
//
//void FrameBuffer::Clear(ID3D11DeviceContext* dc, float r, float g, float b, float a)
//{
//    float color[4]{ r, g, b, a };
//    for (auto& rtv : renderTargetViews)
//    {
//        dc->ClearRenderTargetView(rtv.Get(), color);
//    }
//    dc->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
//}
//
//void FrameBuffer::SetRenderTargets(ID3D11DeviceContext* dc)
//{
//    dc->RSSetViewports(1, &viewport);
//
//    std::vector<ID3D11RenderTargetView*> rtvs;
//    for (auto& rtv : renderTargetViews) {
//        rtvs.push_back(rtv.Get());
//    }
//
//    dc->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), depthStencilView.Get());
//}
//
//void FrameBuffer::SetRenderTarget(ID3D11DeviceContext* dc, ID3D11DepthStencilView* dsv)
//{
//    dc->RSSetViewports(1, &viewport);
//
//    std::vector<ID3D11RenderTargetView*> rtvs;
//    for (auto& rtv : renderTargetViews) {
//        rtvs.push_back(rtv.Get());
//    }
//
//    dc->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), dsv);
//}
#include "FrameBuffer.h"
#include "System/Misc.h"
#include "RHI/ICommandList.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/IResourceFactory.h"
#include "ImGuiRenderer.h"


static TextureFormat ConvertDXGIToTextureFormat(DXGI_FORMAT format) {
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
        // View history / preview framebuffers are sampled and rendered to,
        // but never used as UAVs. Requesting UAV here widens format/flag
        // requirements and was tripping DX12 resource creation in PlayerEditor.
        desc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
        m_colorTextures.push_back(factory->CreateTexture("fb_color", desc));
    }
    desc.format = depthFormat;
    desc.bindFlags = TextureBindFlags::DepthStencil | TextureBindFlags::ShaderResource;
    m_depthTexture = factory->CreateTexture("fb_depth", desc);
}

FrameBuffer::FrameBuffer(ID3D11Device* device, IDXGISwapChain* swapchain)
{
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
    if (index >= m_colorTextures.size()) return nullptr;
    return ImGuiRenderer::GetTextureID(m_colorTextures[index].get());
}

void FrameBuffer::Clear(ICommandList* commandList, float r, float g, float b, float a)
{
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
    commandList->SetViewport(RhiViewport(0.0f, 0.0f, m_width, m_height));

    std::vector<ITexture*> rtvs;
    for (const auto& tex : m_colorTextures) rtvs.push_back(tex.get());

    commandList->SetRenderTargets((uint32_t)rtvs.size(), rtvs.data(), m_depthTexture.get());
}

void FrameBuffer::SetRenderTarget(ICommandList* commandList, ITexture* depthStencil)
{
    commandList->SetViewport(RhiViewport(0.0f, 0.0f, m_width, m_height));

    std::vector<ITexture*> rtvs;
    for (const auto& tex : m_colorTextures) rtvs.push_back(tex.get());

    commandList->SetRenderTargets((uint32_t)rtvs.size(), rtvs.data(), depthStencil);
}
