// DX12PSOCache の RHI 関連インターフェースまたは実装宣言をまとめます。
#pragma once
#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "RHI/PipelineStateDesc.h"
#include <unordered_map>

class DX12PSOCache {
public:
    // device と rootSig は外部所有。PSO compile 時に参照する。
    DX12PSOCache(DX12Device* device, DX12RootSignature* rootSig)
        : m_device(device), m_rootSig(rootSig) {}

    // 同じ PipelineStateDesc の PSO を再利用し、未作成なら compile する。
    ID3D12PipelineState* GetOrCreate(const PipelineStateDesc& desc);

private:
    // PipelineStateDesc の内容から cache key を作る。
    size_t HashDesc(const PipelineStateDesc& desc);
    // shader bytecode / render target format / state desc から native PSO を生成する。
    ComPtr<ID3D12PipelineState> CompilePSO(const PipelineStateDesc& desc);
    // RHI texture format を PSO desc 用 DXGI_FORMAT に変換する。
    DXGI_FORMAT ToDXGIFormat(TextureFormat format);

    DX12Device* m_device;       // PSO を作成する DX12Device。非所有。
    DX12RootSignature* m_rootSig; // PSO に設定する root signature。非所有。
    std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> m_cache; // hash から PSO への cache。
};
