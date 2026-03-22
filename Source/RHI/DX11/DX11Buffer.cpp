#include "DX11Buffer.h"
#include <stdexcept>

DX11Buffer::DX11Buffer(ID3D11Device* device, uint32_t size, BufferType type, const void* initialData, uint32_t stride)
    : m_size(size), m_type(type), m_stride(stride)
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = size;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;

    switch (type) {
    case BufferType::Vertex:
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        break;
    case BufferType::Index:
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        break;
    case BufferType::Indirect:
        desc.BindFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        break;
    case BufferType::Constant:
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.ByteWidth = (size + 15) & ~15;
        break;
    case BufferType::Structured:
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = stride;
        break;
    }

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = initialData;

    HRESULT hr = device->CreateBuffer(&desc, initialData ? &data : nullptr, m_buffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Failed to create DX11Buffer.");
}
