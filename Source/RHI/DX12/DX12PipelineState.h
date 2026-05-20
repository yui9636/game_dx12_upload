// DX12PipelineState の RHI 関連インターフェースまたは実装宣言をまとめます。
#pragma once
#include "RHI/IPipelineState.h"
#include "DX12Device.h"

class DX12PipelineState : public IPipelineState {
public:
    // desc は PSO 再構築や cache key 生成に使う元情報として保持する。
    DX12PipelineState(const PipelineStateDesc& desc, ID3D12PipelineState* pso = nullptr)
        : m_desc(desc), m_pso(pso) {}
    ~DX12PipelineState() override = default;

    const PipelineStateDesc& GetDesc() const override { return m_desc; }
    // DX12CommandList が SetPipelineState へ渡す native PSO。
    ID3D12PipelineState* GetNativePSO() const { return m_pso.Get(); }
    // lazy compile 後に cache された PSO を差し替える。
    void SetNativePSO(ID3D12PipelineState* pso) { m_pso = pso; }

private:
    PipelineStateDesc m_desc;          // RHI 共通の pipeline 設定。
    ComPtr<ID3D12PipelineState> m_pso; // compile 済み native PSO。未作成なら nullptr。
};
