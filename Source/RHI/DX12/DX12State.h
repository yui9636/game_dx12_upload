#pragma once
#include "RHI/IState.h"
#include <d3d12.h>
#include <vector>

// DX12 state objects are thin wrappers holding descriptions.
// Actual GPU state is baked into PSOs by DX12CommandList.

class DX12InputLayout : public IInputLayout {
public:
    DX12InputLayout() = default;
    DX12InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& elements)
        : m_elements(elements) {}
    const D3D12_INPUT_ELEMENT_DESC* GetElements() const { return m_elements.data(); }
    uint32_t GetNumElements() const { return (uint32_t)m_elements.size(); }
private:
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_elements;
};

class DX12DepthStencilState : public IDepthStencilState {
public:
    DX12DepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc) : m_desc(desc) {}
    const D3D12_DEPTH_STENCIL_DESC& GetDesc() const { return m_desc; }
private:
    D3D12_DEPTH_STENCIL_DESC m_desc;
};

class DX12RasterizerState : public IRasterizerState {
public:
    DX12RasterizerState(const D3D12_RASTERIZER_DESC& desc) : m_desc(desc) {}
    const D3D12_RASTERIZER_DESC& GetDesc() const { return m_desc; }
private:
    D3D12_RASTERIZER_DESC m_desc;
};

class DX12BlendState : public IBlendState {
public:
    DX12BlendState(const D3D12_BLEND_DESC& desc) : m_desc(desc) {}
    const D3D12_BLEND_DESC& GetDesc() const { return m_desc; }
private:
    D3D12_BLEND_DESC m_desc;
};
