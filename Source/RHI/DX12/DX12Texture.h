#pragma once
#include "RHI/ITexture.h"
#include "DX12Device.h"

class DX12Texture : public ITexture {
public:
    // Transient / created texture
    DX12Texture(DX12Device* device, uint32_t width, uint32_t height,
                TextureFormat format, TextureBindFlags bindFlags);

    // Back buffer wrapper
    DX12Texture(DX12Device* device, ID3D12Resource* backBuffer, uint32_t index);

    // File-loaded texture (SRV only, from upload)
    DX12Texture(DX12Device* device, ComPtr<ID3D12Resource> resource,
                uint32_t width, uint32_t height, DXGI_FORMAT srvFormat,
                bool isCubemap = false);

    // Texture array (full resource + SRV)
    DX12Texture(DX12Device* device, uint32_t width, uint32_t height,
                TextureFormat format, uint32_t arraySize,
                TextureBindFlags bindFlags);

    // Texture array slice (shared resource DSV slice)
    DX12Texture(DX12Device* device, ComPtr<ID3D12Resource> sharedResource,
                uint32_t width, uint32_t height, uint32_t arraySlice);

    ~DX12Texture() override = default;

    uint32_t GetWidth()  const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }
    TextureFormat GetFormat() const override { return m_format; }
    ResourceState GetCurrentState() const override { return m_currentState; }
    void SetCurrentState(ResourceState state) override { m_currentState = state; }

    // DX12 native access
    ID3D12Resource*             GetNativeResource() const { return m_resource.Get(); }
    ComPtr<ID3D12Resource>      GetNativeResourceComPtr() const { return m_resource; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_rtvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_dsvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
    bool HasRTV() const { return m_hasRTV; }
    bool HasDSV() const { return m_hasDSV; }
    bool HasSRV() const { return m_hasSRV; }

private:
    DXGI_FORMAT ToDXGIFormat(TextureFormat format);
    D3D12_RESOURCE_FLAGS ToResourceFlags(TextureBindFlags flags);

    ComPtr<ID3D12Resource> m_resource;
    uint32_t m_width = 0, m_height = 0;
    TextureFormat m_format = TextureFormat::Unknown;
    ResourceState m_currentState = ResourceState::Common;

    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle = {};
    bool m_hasRTV = false, m_hasDSV = false, m_hasSRV = false;
};
