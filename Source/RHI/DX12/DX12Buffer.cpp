#include "DX12Buffer.h"
#include <cassert>
#include <cstring>

DX12Buffer::DX12Buffer(DX12Device* device, uint32_t size, BufferType type, const void* initialData)
    : m_size(size), m_type(type)
{
    // Constant buffers must be 256-byte aligned
    uint32_t alignedSize = size;
    if (type == BufferType::Constant) {
        alignedSize = (size + 255) & ~255;
        m_size = alignedSize;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = alignedSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_resource));
    assert(SUCCEEDED(hr));

    if (initialData) {
        void* mapped = Map();
        if (mapped) {
            memcpy(mapped, initialData, size);
            Unmap();
        }
    }
}

void* DX12Buffer::Map() {
    if (!m_mappedData) {
        D3D12_RANGE readRange = { 0, 0 }; // We don't read from this resource on CPU
        HRESULT hr = m_resource->Map(0, &readRange, &m_mappedData);
        if (FAILED(hr)) return nullptr;
    }
    return m_mappedData;
}

void DX12Buffer::Unmap() {
    if (m_mappedData) {
        m_resource->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
}
