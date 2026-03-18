#pragma once
#include "../IBuffer.h"
#include <d3d11.h>
#include <wrl.h>

class DX11Buffer : public IBuffer {
public:
    DX11Buffer(ID3D11Device* device, uint32_t size, BufferType type, const void* initialData = nullptr);
    ~DX11Buffer() override = default;

    uint32_t GetSize() const override { return m_size; }
    BufferType GetType() const override { return m_type; }

    ID3D11Buffer* GetNative() const { return m_buffer.Get(); }

    DX11Buffer(ID3D11Buffer* buffer) : m_buffer(buffer) {}
private:
    uint32_t m_size;
    BufferType m_type;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
};