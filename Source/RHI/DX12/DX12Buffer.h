#pragma once

#include "RHI/IBuffer.h"
#include "DX12Device.h"

class DX12Buffer : public IBuffer {
public:
    DX12Buffer(DX12Device* device, uint32_t size, BufferType type, const void* initialData = nullptr, uint32_t stride = 0);
    ~DX12Buffer() override = default;

    uint32_t GetSize() const override { return m_size; }
    BufferType GetType() const override { return m_type; }
    uint32_t GetStride() const override { return m_stride; }

    ID3D12Resource* GetNativeResource() const { return m_resource.Get(); }
    bool IsValid() const { return m_resource.Get() != nullptr; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const {
        return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
    }

    void* Map();
    void Unmap();

private:
    uint32_t m_size;
    BufferType m_type;
    uint32_t m_stride = 0;
    ComPtr<ID3D12Resource> m_resource;
    void* m_mappedData = nullptr;
};
