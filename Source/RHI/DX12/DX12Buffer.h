#pragma once
#include "RHI/IBuffer.h"
#include "DX12Device.h"

class DX12Buffer : public IBuffer {
public:
    DX12Buffer(DX12Device* device, uint32_t size, BufferType type, const void* initialData = nullptr);
    ~DX12Buffer() override = default;

    uint32_t GetSize() const override { return m_size; }
    BufferType GetType() const override { return m_type; }

    ID3D12Resource* GetNativeResource() const { return m_resource.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_resource->GetGPUVirtualAddress(); }

    void* Map();
    void Unmap();

private:
    uint32_t m_size;
    BufferType m_type;
    ComPtr<ID3D12Resource> m_resource;
    void* m_mappedData = nullptr;
};
