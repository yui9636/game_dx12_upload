#pragma once
#include "RHI/IState.h"
#include <d3d12.h>
#include <vector>

// DX12 state object は description を保持する薄い wrapper。
// 実際の GPU state は DX12CommandList が PSO へ焼き込む。

class DX12InputLayout : public IInputLayout {
public:
    DX12InputLayout() = default;
    // RHI の InputLayoutDesc から変換済みの DX12 element 配列を保持する。
    DX12InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& elements)
        : m_elements(elements) {}
    const D3D12_INPUT_ELEMENT_DESC* GetElements() const { return m_elements.data(); }
    uint32_t GetNumElements() const { return (uint32_t)m_elements.size(); }
private:
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_elements; // PSO 作成時に渡す input element 配列。
};

class DX12DepthStencilState : public IDepthStencilState {
public:
    // depth / stencil test の設定を PSO に焼き込むため保持する。
    DX12DepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc) : m_desc(desc) {}
    const D3D12_DEPTH_STENCIL_DESC& GetDesc() const { return m_desc; }
private:
    D3D12_DEPTH_STENCIL_DESC m_desc; // DX12 native depth-stencil desc。
};

class DX12RasterizerState : public IRasterizerState {
public:
    // fill mode / cull mode / depth bias などの rasterizer 設定を保持する。
    DX12RasterizerState(const D3D12_RASTERIZER_DESC& desc) : m_desc(desc) {}
    const D3D12_RASTERIZER_DESC& GetDesc() const { return m_desc; }
private:
    D3D12_RASTERIZER_DESC m_desc; // DX12 native rasterizer desc。
};

class DX12BlendState : public IBlendState {
public:
    // render target ごとの blend 設定を保持する。
    DX12BlendState(const D3D12_BLEND_DESC& desc) : m_desc(desc) {}
    const D3D12_BLEND_DESC& GetDesc() const { return m_desc; }
private:
    D3D12_BLEND_DESC m_desc; // DX12 native blend desc。
};
