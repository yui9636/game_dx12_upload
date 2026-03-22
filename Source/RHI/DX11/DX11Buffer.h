#pragma once

#include "../IBuffer.h"
#include <d3d11.h>
#include <wrl.h>

class DX11Buffer : public IBuffer {
public:
    DX11Buffer(ID3D11Device* device, uint32_t size, BufferType type, const void* initialData = nullptr, uint32_t stride = 0);
    ~DX11Buffer() override = default;

    uint32_t GetSize() const override { return m_size; }
    BufferType GetType() const override { return m_type; }
    uint32_t GetStride() const override { return m_stride; }

    ID3D11Buffer* GetNative() const { return m_buffer.Get(); }

    DX11Buffer(ID3D11Buffer* buffer) : m_buffer(buffer) {}
private:
    uint32_t m_size = 0;
    BufferType m_type = BufferType::Vertex;
    uint32_t m_stride = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
};
