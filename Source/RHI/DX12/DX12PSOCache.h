#pragma once
#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "RHI/PipelineStateDesc.h"
#include <unordered_map>

class DX12PSOCache {
public:
    DX12PSOCache(DX12Device* device, DX12RootSignature* rootSig)
        : m_device(device), m_rootSig(rootSig) {}

    ID3D12PipelineState* GetOrCreate(const PipelineStateDesc& desc);

private:
    size_t HashDesc(const PipelineStateDesc& desc);
    ComPtr<ID3D12PipelineState> CompilePSO(const PipelineStateDesc& desc);
    DXGI_FORMAT ToDXGIFormat(TextureFormat format);

    DX12Device* m_device;
    DX12RootSignature* m_rootSig;
    std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> m_cache;
};
