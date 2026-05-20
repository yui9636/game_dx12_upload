// DX12Buffer の RHI 関連インターフェースまたは実装宣言をまとめます。
#pragma once

#include "RHI/IBuffer.h"
#include "DX12Device.h"

class DX12Buffer : public IBuffer {
public:
    // type に応じて UPLOAD heap か DEFAULT heap を選び、必要なら初期データを転送する。
    DX12Buffer(DX12Device* device, uint32_t size, BufferType type, const void* initialData = nullptr, uint32_t stride = 0);
    ~DX12Buffer() override = default;

    uint32_t GetSize() const override { return m_size; }
    BufferType GetType() const override { return m_type; }
    uint32_t GetStride() const override { return m_stride; }

    ID3D12Resource* GetNativeResource() const { return m_resource.Get(); }
    bool IsValid() const { return m_resource.Get() != nullptr; }
    // CBV/SRV/UAV や root descriptor に渡す GPU virtual address。
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const {
        return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
    }

    // UPLOAD heap buffer を CPU から書き込む。DEFAULT heap の UAVStorage では nullptr を返す。
    void* Map();
    void Unmap();

private:
    uint32_t m_size;                   // buffer の byte size。Constant は 256 byte alignment 済み。
    BufferType m_type;                 // RHI 側の buffer 種別。
    uint32_t m_stride = 0;             // StructuredBuffer の 1 element size。
    ComPtr<ID3D12Resource> m_resource; // DX12 buffer resource 本体。
    void* m_mappedData = nullptr;      // Map 済み CPU pointer。未 Map なら nullptr。
};
