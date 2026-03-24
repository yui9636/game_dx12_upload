#pragma once
#include "DX12Device.h"

// Linear descriptor allocator (reset each frame)
class DX12DescriptorAllocator {
public:
    DX12DescriptorAllocator(ID3D12Device* device,
                            D3D12_DESCRIPTOR_HEAP_TYPE type,
                            uint32_t maxDescriptors,
                            bool shaderVisible = true);
    ~DX12DescriptorAllocator() = default;

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPU();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateBlock(uint32_t count);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleAtOffset(D3D12_CPU_DESCRIPTOR_HANDLE base, uint32_t offset) const;
    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

    void Reset() { m_current = 0; }
    uint32_t GetCount() const { return m_current; }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    uint32_t m_descriptorSize = 0;
    uint32_t m_current = 0;
    uint32_t m_max = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};
};
