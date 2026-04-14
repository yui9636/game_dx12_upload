#include "DX12Texture.h"
#include "Graphics.h"
#include "Console/Logger.h"
#include <cassert>

DXGI_FORMAT DX12Texture::ToDXGIFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::RGBA8_UNORM:          return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R16G16B16A16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R32G32B32A32_FLOAT:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::R32G32B32A32_UINT:    return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R32G32B32_FLOAT:      return DXGI_FORMAT_R32G32B32_FLOAT;
    case TextureFormat::R32G32_FLOAT:         return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::R16G16_FLOAT:         return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::R8_UNORM:             return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::D32_FLOAT:            return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::D24_UNORM_S8_UINT:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::R32_TYPELESS:         return DXGI_FORMAT_R32_TYPELESS;
    default:                                  return DXGI_FORMAT_UNKNOWN;
    }
}

D3D12_RESOURCE_FLAGS DX12Texture::ToResourceFlags(TextureBindFlags flags) {
    D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;
    if (flags & TextureBindFlags::RenderTarget)    result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (flags & TextureBindFlags::DepthStencil)    result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (flags & TextureBindFlags::UnorderedAccess) result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    // If depth-only (no SRV), deny SRV
    if ((flags & TextureBindFlags::DepthStencil) && !(flags & TextureBindFlags::ShaderResource)) {
        result |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    return result;
}

DX12Texture::DX12Texture(DX12Device* device, uint32_t width, uint32_t height,
                         TextureFormat format, TextureBindFlags bindFlags,
                         const float* optimizedClearColor)
    : m_width(width), m_height(height), m_format(format), m_device(device)
{
    auto* d3dDevice = device->GetDevice();

    // Determine DXGI format
    DXGI_FORMAT dxgiFormat = ToDXGIFormat(format);
    DXGI_FORMAT resourceFormat = dxgiFormat;
    DXGI_FORMAT dsvFormat = dxgiFormat;
    DXGI_FORMAT srvFormat = dxgiFormat;

    // Use typeless for depth formats that also need SRV
    bool isDepth = (format == TextureFormat::D32_FLOAT || format == TextureFormat::D24_UNORM_S8_UINT
                    || format == TextureFormat::R32_TYPELESS);
    if (isDepth) {
        if (format == TextureFormat::D24_UNORM_S8_UINT) {
            dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            resourceFormat = (bindFlags & TextureBindFlags::ShaderResource)
                ? DXGI_FORMAT_R24G8_TYPELESS
                : DXGI_FORMAT_D24_UNORM_S8_UINT;
        } else {
            dsvFormat = DXGI_FORMAT_D32_FLOAT;
            srvFormat = DXGI_FORMAT_R32_FLOAT;
            resourceFormat = (bindFlags & TextureBindFlags::ShaderResource)
                ? DXGI_FORMAT_R32_TYPELESS
                : dxgiFormat;
        }
    }

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = resourceFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = ToResourceFlags(bindFlags);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;

    if (isDepth) {
        clearValue.Format = dsvFormat;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        pClearValue = &clearValue;
        m_currentState = ResourceState::DepthWrite;
    } else if (bindFlags & TextureBindFlags::RenderTarget) {
        clearValue.Format = dxgiFormat;
        if (optimizedClearColor) {
            clearValue.Color[0] = optimizedClearColor[0];
            clearValue.Color[1] = optimizedClearColor[1];
            clearValue.Color[2] = optimizedClearColor[2];
            clearValue.Color[3] = optimizedClearColor[3];
        } else {
            clearValue.Color[0] = 0.0f; clearValue.Color[1] = 0.0f;
            clearValue.Color[2] = 0.0f; clearValue.Color[3] = 0.0f;
        }
        pClearValue = &clearValue;
        m_currentState = ResourceState::RenderTarget;
    }

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if (isDepth) initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    else if (bindFlags & TextureBindFlags::RenderTarget) initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    HRESULT hr = d3dDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, initialState, pClearValue,
        IID_PPV_ARGS(&m_resource));
    if (FAILED(hr) && pClearValue != nullptr) {
        hr = d3dDevice->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &texDesc, initialState, nullptr,
            IID_PPV_ARGS(&m_resource));
    }
    if (FAILED(hr)) {
        LOG_ERROR("[DX12Texture] CreateCommittedResource failed. w=%u h=%u format=%d bind=%u flags=%u hr=0x%08X",
            width,
            height,
            static_cast<int>(format),
            static_cast<uint32_t>(bindFlags),
            static_cast<uint32_t>(texDesc.Flags),
            static_cast<unsigned int>(hr));
    }
    assert(SUCCEEDED(hr));

    // Create views
    // RTV
    if (bindFlags & TextureBindFlags::RenderTarget) {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = dxgiFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_rtvHandle = device->AllocateRTVDescriptor();
        d3dDevice->CreateRenderTargetView(m_resource.Get(), &rtvDesc, m_rtvHandle);
        m_hasRTV = true;
    }

    // DSV
    if (bindFlags & TextureBindFlags::DepthStencil) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dsvFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        m_dsvHandle = device->AllocateDSVDescriptor();
        d3dDevice->CreateDepthStencilView(m_resource.Get(), &dsvDesc, m_dsvHandle);
        m_hasDSV = true;
    }

    // SRV
    if (bindFlags & TextureBindFlags::ShaderResource) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        if (isDepth) srvDesc.Format = srvFormat;
        else         srvDesc.Format = dxgiFormat;

        m_srvHandle = device->AllocateSRVDescriptor();
        d3dDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle);
        m_hasSRV = true;
    }
}

DX12Texture::DX12Texture(DX12Device* device, ID3D12Resource* backBuffer, uint32_t index)
    : m_format(TextureFormat::RGBA8_UNORM)
    , m_currentState(ResourceState::Present)
    , m_device(device)
{
    m_resource = backBuffer; // AddRef via ComPtr assignment

    auto desc = backBuffer->GetDesc();
    m_width = static_cast<uint32_t>(desc.Width);
    m_height = desc.Height;

    // Create RTV for back buffer
    auto* d3dDevice = device->GetDevice();
    m_rtvHandle = device->GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
    m_rtvHandle.ptr += static_cast<SIZE_T>(index) * device->GetRTVDescriptorSize();
    d3dDevice->CreateRenderTargetView(backBuffer, nullptr, m_rtvHandle);
    m_hasRTV = true;
}

// ========================================================
// File-loaded texture (SRV only, from upload heap)
// ========================================================
DX12Texture::DX12Texture(DX12Device* device, ComPtr<ID3D12Resource> resource,
                         uint32_t width, uint32_t height, DXGI_FORMAT srvFormat,
                         bool isCubemap)
    : m_resource(resource)
    , m_width(width), m_height(height)
    , m_format(TextureFormat::RGBA8_UNORM) // approximate - actual format is srvFormat
    , m_currentState(ResourceState::ShaderResource)
    , m_device(device)
{
    auto* d3dDevice = device->GetDevice();
    auto resDesc = resource->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Texture3D.MipLevels = resDesc.MipLevels;
        srvDesc.Texture3D.MostDetailedMip = 0;
        srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
    } else if (isCubemap) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = resDesc.MipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
    } else if (resDesc.DepthOrArraySize > 1) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = resDesc.MipLevels;
        srvDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = resDesc.MipLevels;
    }

    m_srvHandle = device->AllocateSRVDescriptor();
    d3dDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle);
    m_hasSRV = true;
}

// ========================================================
// Texture array (full resource + SRV)
// ========================================================
DX12Texture::DX12Texture(DX12Device* device, uint32_t width, uint32_t height,
                         TextureFormat format, uint32_t arraySize,
                         TextureBindFlags bindFlags)
    : m_width(width), m_height(height), m_format(format), m_device(device)
{
    auto* d3dDevice = device->GetDevice();

    DXGI_FORMAT dxgiFormat = ToDXGIFormat(format);
    bool isDepth = (format == TextureFormat::D32_FLOAT || format == TextureFormat::D24_UNORM_S8_UINT
                    || format == TextureFormat::R32_TYPELESS);
    DXGI_FORMAT resourceFormat = dxgiFormat;
    DXGI_FORMAT dsvFormat = dxgiFormat;
    DXGI_FORMAT srvFormat = dxgiFormat;
    if (isDepth) {
        if (format == TextureFormat::D24_UNORM_S8_UINT) {
            dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            resourceFormat = (bindFlags & TextureBindFlags::ShaderResource)
                ? DXGI_FORMAT_R24G8_TYPELESS
                : DXGI_FORMAT_D24_UNORM_S8_UINT;
        } else {
            dsvFormat = DXGI_FORMAT_D32_FLOAT;
            srvFormat = DXGI_FORMAT_R32_FLOAT;
            resourceFormat = (bindFlags & TextureBindFlags::ShaderResource)
                ? DXGI_FORMAT_R32_TYPELESS
                : dxgiFormat;
        }
    }

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = static_cast<UINT16>(arraySize);
    texDesc.MipLevels = 1;
    texDesc.Format = resourceFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = ToResourceFlags(bindFlags);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    if (isDepth) {
        clearValue.Format = dsvFormat;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        pClearValue = &clearValue;
        initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        m_currentState = ResourceState::DepthWrite;
    }

    HRESULT hr = d3dDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, initialState, pClearValue,
        IID_PPV_ARGS(&m_resource));
    assert(SUCCEEDED(hr));

    // SRV for the entire array
    if (bindFlags & TextureBindFlags::ShaderResource) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Format = isDepth ? srvFormat : dxgiFormat;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.ArraySize = arraySize;
        srvDesc.Texture2DArray.FirstArraySlice = 0;

        m_srvHandle = device->AllocateSRVDescriptor();
        d3dDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle);
        m_hasSRV = true;
    }

    // Note: DSV is NOT created here - each slice has its own DSV (see array slice constructor)
}

// ========================================================
// Texture array slice (shared resource, DSV only)
// ========================================================
DX12Texture::DX12Texture(DX12Device* device, ComPtr<ID3D12Resource> sharedResource,
                         uint32_t width, uint32_t height, uint32_t arraySlice)
    : m_resource(sharedResource)
    , m_width(width), m_height(height)
    , m_format(TextureFormat::R32_TYPELESS)
    , m_currentState(ResourceState::DepthWrite)
    , m_device(device)
{
    auto* d3dDevice = device->GetDevice();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
    dsvDesc.Texture2DArray.ArraySize = 1;
    dsvDesc.Texture2DArray.MipSlice = 0;

    m_dsvHandle = device->AllocateDSVDescriptor();
    d3dDevice->CreateDepthStencilView(m_resource.Get(), &dsvDesc, m_dsvHandle);
    m_hasDSV = true;
}

DX12Texture::~DX12Texture()
{
    if (Graphics::IsShuttingDown()) {
        return;
    }

    if (!m_device || !m_retireFence || m_retireFenceValue == 0) {
        return;
    }

    if (m_hasSRV && m_srvHandle.ptr) {
        m_device->DeferFreeDescriptor(m_srvHandle, m_retireFence, m_retireFenceValue, DX12Device::DescriptorType::SRV);
        m_srvHandle.ptr = 0;
        m_hasSRV = false;
    }
    if (m_hasRTV && m_rtvHandle.ptr) {
        m_device->DeferFreeDescriptor(m_rtvHandle, m_retireFence, m_retireFenceValue, DX12Device::DescriptorType::RTV);
        m_rtvHandle.ptr = 0;
        m_hasRTV = false;
    }
    if (m_hasDSV && m_dsvHandle.ptr) {
        m_device->DeferFreeDescriptor(m_dsvHandle, m_retireFence, m_retireFenceValue, DX12Device::DescriptorType::DSV);
        m_dsvHandle.ptr = 0;
        m_hasDSV = false;
    }
}
