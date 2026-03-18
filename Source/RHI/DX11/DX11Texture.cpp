#include "DX11Texture.h"
#include <stdexcept>

DX11Texture::DX11Texture(ID3D11Device* device, uint32_t width, uint32_t height, TextureFormat format, TextureBindFlags bindFlags)
    : m_width(width), m_height(height), m_format(format)
{
    DXGI_FORMAT dxgiFormat = GetDXGIFormat(format);

    // ����Ή��F�[�x�o�b�t�@��SRV�Ƃ��ēǂޏꍇ�́ATYPELESS�Ńe�N�X�`�������K�v������
    DXGI_FORMAT texFormat = dxgiFormat;
    if ((bindFlags & TextureBindFlags::DepthStencil) && (bindFlags & TextureBindFlags::ShaderResource)) {
        if (format == TextureFormat::D32_FLOAT) texFormat = DXGI_FORMAT_R32_TYPELESS;
        else if (format == TextureFormat::D24_UNORM_S8_UINT) texFormat = DXGI_FORMAT_R24G8_TYPELESS;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = texFormat;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;

    // �o�C���h�t���O�̕ϊ�
    if (bindFlags & TextureBindFlags::ShaderResource) desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (bindFlags & TextureBindFlags::RenderTarget)   desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (bindFlags & TextureBindFlags::DepthStencil)   desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    if (bindFlags & TextureBindFlags::UnorderedAccess)desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Failed to create DX11Texture.");

    // SRV�̍쐬
    if (bindFlags & TextureBindFlags::ShaderResource) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // �[�x��SRV�ɂ���ꍇ�̃t�H�[�}�b�g�␳
        if (format == TextureFormat::D32_FLOAT) srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        else if (format == TextureFormat::D24_UNORM_S8_UINT) srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        else srvDesc.Format = dxgiFormat;

        device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srv.GetAddressOf());
    }

    // RTV�̍쐬
    if (bindFlags & TextureBindFlags::RenderTarget) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = dxgiFormat;
        device->CreateRenderTargetView(m_texture.Get(), &rtvDesc, m_rtv.GetAddressOf());
    }

    // DSV�̍쐬
    if (bindFlags & TextureBindFlags::DepthStencil) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = dxgiFormat;
        device->CreateDepthStencilView(m_texture.Get(), &dsvDesc, m_dsv.GetAddressOf());
    }
}


DX11Texture::DX11Texture(ID3D11ShaderResourceView* srv)
    : m_srv(srv)
{
    m_currentState = ResourceState::ShaderResource;

    if (srv) {
        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        srv->GetResource(res.GetAddressOf());

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
        if (SUCCEEDED(res.As(&tex2D))) {
            m_texture = tex2D;
            D3D11_TEXTURE2D_DESC desc;
            tex2D->GetDesc(&desc);

            m_width = desc.Width;
            m_height = desc.Height;
            // ���t�H�[�}�b�g�͌����ȋt�ϊ����K�v�ȏꍇ�������A��U Unknown �ň����܂�
            m_format = TextureFormat::Unknown;
        }
    }
}


DXGI_FORMAT DX11Texture::GetDXGIFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::RGBA8_UNORM:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::R32G32B32A32_UINT:  return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R32G32B32_FLOAT:    return DXGI_FORMAT_R32G32B32_FLOAT;
    case TextureFormat::R32G32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::R16G16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::R8_UNORM:           return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::D32_FLOAT:          return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::D24_UNORM_S8_UINT:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

DX11Texture::DX11Texture(ID3D11Device* device, IDXGISwapChain* swapchain)
{
    // �X���b�v�`�F�[������e�N�X�`���������������ă��b�v����
    swapchain->GetBuffer(0, IID_PPV_ARGS(m_texture.GetAddressOf()));
    D3D11_TEXTURE2D_DESC desc;
    m_texture->GetDesc(&desc);
    m_width = desc.Width;
    m_height = desc.Height;
    m_format = TextureFormat::RGBA8_UNORM;

    device->CreateRenderTargetView(m_texture.Get(), nullptr, m_rtv.GetAddressOf());
}

DX11Texture::DX11Texture(ID3D11DepthStencilView* dsv, uint32_t width, uint32_t height)
    : m_dsv(dsv), m_width(width), m_height(height), m_format(TextureFormat::Unknown)
{
    m_currentState = ResourceState::DepthWrite;
}

DX11Texture::DX11Texture(ID3D11RenderTargetView* rtv, uint32_t width, uint32_t height)
    : m_rtv(rtv), m_width(width), m_height(height), m_format(TextureFormat::Unknown)
{
    m_currentState = ResourceState::RenderTarget;
}