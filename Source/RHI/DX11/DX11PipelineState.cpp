#include "DX11PipelineState.h"

DX11PipelineState::DX11PipelineState(const PipelineStateDesc& desc)
    : m_desc(desc)
{

}

DX11PipelineState::~DX11PipelineState()
{
}

const PipelineStateDesc& DX11PipelineState::GetDesc() const
{
    return m_desc;
}
