#pragma once
#include "DX12Device.h"

// 毎 frame reset する linear descriptor allocator。
// DX12CommandList が frame-local な SRV table を作るために使う。
class DX12DescriptorAllocator {
public:
    // maxDescriptors 分の descriptor heap を作る。
    // shaderVisible=true の場合は GPU handle も有効になる。
    DX12DescriptorAllocator(ID3D12Device* device,
                            D3D12_DESCRIPTOR_HEAP_TYPE type,
                            uint32_t maxDescriptors,
                            bool shaderVisible = true);
    ~DX12DescriptorAllocator() = default;

    // 1 descriptor を線形確保する。
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPU();
    // count 個の連続 descriptor block を線形確保する。
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateBlock(uint32_t count);
    // 同じ offset の GPU handle を返す。shader visible heap でだけ意味を持つ。
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;
    // base から offset 個進めた CPU handle を返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandleAtOffset(D3D12_CPU_DESCRIPTOR_HANDLE base, uint32_t offset) const;
    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

    // frame 終了後に先頭から再利用する。
    void Reset() { m_current = 0; }
    uint32_t GetCount() const { return m_current; }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;      // allocator が所有する descriptor heap。
    uint32_t m_descriptorSize = 0;            // 1 descriptor あたりの increment size。
    uint32_t m_current = 0;                   // 次に確保する descriptor index。
    uint32_t m_max = 0;                       // heap 内の最大 descriptor 数。
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {}; // heap 先頭の CPU handle。
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {}; // heap 先頭の GPU handle。
};
