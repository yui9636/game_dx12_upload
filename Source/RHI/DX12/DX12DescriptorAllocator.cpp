// DX12DescriptorAllocator の RHI バックエンド実装をまとめます。
#include "DX12DescriptorAllocator.h"
#include <cassert>

DX12DescriptorAllocator::DX12DescriptorAllocator(
    ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t maxDescriptors,
    bool shaderVisible)
    : m_max(maxDescriptors)
{
    // frame 中に直線的に消費する小さな descriptor heap を作る。
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = maxDescriptors;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                               : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    assert(SUCCEEDED(hr));

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorAllocator::AllocateCPU() {
    // 1 個だけ進める bump allocation。Reset() でまとめて再利用する。
    assert(m_current < m_max && "Descriptor allocator out of space");
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStart;
    handle.ptr += static_cast<SIZE_T>(m_current) * m_descriptorSize;
    m_current++;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorAllocator::AllocateBlock(uint32_t count) {
    // texture table のように連続 descriptor が必要な場合に使う。
    assert(m_current + count <= m_max && "Descriptor allocator out of space");
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStart;
    handle.ptr += static_cast<SIZE_T>(m_current) * m_descriptorSize;
    m_current += count;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorAllocator::GetCPUHandleAtOffset(
    D3D12_CPU_DESCRIPTOR_HANDLE base, uint32_t offset) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = base;
    handle.ptr += static_cast<SIZE_T>(offset) * m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorAllocator::GetGPUHandle(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const
{
    // CPU handle の heap 内 offset を、そのまま shader-visible GPU handle へ変換する。
    SIZE_T offset = cpuHandle.ptr - m_cpuStart.ptr;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_gpuStart;
    gpuHandle.ptr += offset;
    return gpuHandle;
}
