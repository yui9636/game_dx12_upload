#pragma once
#include "RHI/IPipelineState.h"
#include "DX12Device.h"

class DX12PipelineState : public IPipelineState {
public:
    DX12PipelineState(const PipelineStateDesc& desc, ID3D12PipelineState* pso = nullptr)
        : m_desc(desc), m_pso(pso) {}
    ~DX12PipelineState() override = default;

    const PipelineStateDesc& GetDesc() const override { return m_desc; }
    ID3D12PipelineState* GetNativePSO() const { return m_pso.Get(); }
    void SetNativePSO(ID3D12PipelineState* pso) { m_pso = pso; }

private:
    PipelineStateDesc m_desc;
    ComPtr<ID3D12PipelineState> m_pso;
};
