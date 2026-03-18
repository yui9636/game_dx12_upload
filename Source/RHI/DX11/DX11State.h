#pragma once
#include "RHI/IState.h"
#include <d3d11.h>
#include <wrl.h>


class DX11InputLayout : public IInputLayout {
public:
    DX11InputLayout(ID3D11InputLayout* layout) : m_layout(layout) {}
    ID3D11InputLayout* GetNative() const { return m_layout.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
};

class DX11DepthStencilState : public IDepthStencilState {
public:
    DX11DepthStencilState(ID3D11DepthStencilState* state) : m_state(state) {}
    ID3D11DepthStencilState* GetNative() const { return m_state.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_state;
};

class DX11RasterizerState : public IRasterizerState {
public:
    DX11RasterizerState(ID3D11RasterizerState* state) : m_state(state) {}
    ID3D11RasterizerState* GetNative() const { return m_state.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_state;
};

class DX11BlendState : public IBlendState {
public:
    DX11BlendState(ID3D11BlendState* state) : m_state(state) {}
    ID3D11BlendState* GetNative() const { return m_state.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_state;
};