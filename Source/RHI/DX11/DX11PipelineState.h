#pragma once
#include "RHI/IPipelineState.h"

class DX11PipelineState : public IPipelineState {
public:
    DX11PipelineState(const PipelineStateDesc& desc);
    ~DX11PipelineState() override;

    const PipelineStateDesc& GetDesc() const override;

private:
    PipelineStateDesc m_desc;
};