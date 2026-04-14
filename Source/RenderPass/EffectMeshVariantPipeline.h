#pragma once

#include <cstdint>
#include <vector>
#include <wrl/client.h>
#include <d3d12.h>

class DX12Device;

struct MeshVariantPSOEntry
{
    uint32_t variantKey = 0;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
};

struct MeshVariantPipelineCache
{
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    std::vector<MeshVariantPSOEntry> entries;

    ID3D12PipelineState* GetOrCreate(DX12Device* device, uint32_t variantKey);
    void Reset();
};

MeshVariantPipelineCache& GetMeshVariantPipelineCache();
