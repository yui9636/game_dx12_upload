#pragma once

#include <cstdint>
#include <vector>
#include <wrl/client.h>
#include <d3d12.h>

class DX12Device;

// shader variant key と、それに対応する DX12 PSO。
struct MeshVariantPSOEntry
{
    uint32_t variantKey = 0;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
};

// EffectMesh の pixel shader variant を key ごとに遅延生成・キャッシュする。
struct MeshVariantPipelineCache
{
    // variant 間で共有する root signature。
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    // 生成済み PSO の線形キャッシュ。variant 数が少ない前提。
    std::vector<MeshVariantPSOEntry> entries;

    // 既存 PSO を探し、無ければ shader blob を読んで作成する。
    ID3D12PipelineState* GetOrCreate(DX12Device* device, uint32_t variantKey);
    // device reset / shader reload 用に全 variant を破棄する。
    void Reset();
};

MeshVariantPipelineCache& GetMeshVariantPipelineCache();
