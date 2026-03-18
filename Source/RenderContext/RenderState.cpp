#include "System/Misc.h"
#include "RenderState.h"
#include "RHI/ISampler.h"
#include "RHI/DX11/DX11Sampler.h"
#include "RHI/IState.h"
#include "RHI/DX11/DX11State.h"
#include "RHI/DX12/DX12State.h"
#include "RHI/DX12/DX12Device.h"

RenderState::~RenderState() = default;

// ============================================================
// DX12 コンストラクタ
// ============================================================
RenderState::RenderState(DX12Device* /*device*/)
{
	// サンプラー: DX12ではスタティックサンプラーで代替 → nullptr

	// DepthStencilState
	{ D3D12_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = TRUE; d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	  depthStencilStates[static_cast<int>(DepthState::TestAndWrite)] = std::make_unique<DX12DepthStencilState>(d); }
	{ D3D12_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = TRUE; d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	  depthStencilStates[static_cast<int>(DepthState::TestOnly)] = std::make_unique<DX12DepthStencilState>(d); }
	{ D3D12_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = TRUE; d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	  depthStencilStates[static_cast<int>(DepthState::WriteOnly)] = std::make_unique<DX12DepthStencilState>(d); }
	{ D3D12_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = FALSE; d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	  depthStencilStates[static_cast<int>(DepthState::NoTestNoWrite)] = std::make_unique<DX12DepthStencilState>(d); }

	// BlendState
	auto mkB = [](bool en, D3D12_BLEND s, D3D12_BLEND d, D3D12_BLEND_OP o, D3D12_BLEND sa, D3D12_BLEND da, D3D12_BLEND_OP oa) {
		D3D12_BLEND_DESC bd = {}; auto& rt = bd.RenderTarget[0];
		rt.BlendEnable = en; rt.SrcBlend = s; rt.DestBlend = d; rt.BlendOp = o;
		rt.SrcBlendAlpha = sa; rt.DestBlendAlpha = da; rt.BlendOpAlpha = oa;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; return bd; };

	blendStates[static_cast<int>(BlendState::Opaque)]       = std::make_unique<DX12BlendState>(mkB(false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD));
	blendStates[static_cast<int>(BlendState::Transparency)] = std::make_unique<DX12BlendState>(mkB(true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD));
	blendStates[static_cast<int>(BlendState::Additive)]     = std::make_unique<DX12BlendState>(mkB(true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD));
	blendStates[static_cast<int>(BlendState::Subtraction)]  = std::make_unique<DX12BlendState>(mkB(true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_REV_SUBTRACT, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD));
	blendStates[static_cast<int>(BlendState::Multiply)]     = std::make_unique<DX12BlendState>(mkB(true, D3D12_BLEND_ZERO, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD));
	blendStates[static_cast<int>(BlendState::Alpha)]        = std::make_unique<DX12BlendState>(mkB(true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD));

	// RasterizerState
	auto mkR = [](D3D12_FILL_MODE f, D3D12_CULL_MODE c, bool aa = false) {
		D3D12_RASTERIZER_DESC r = {}; r.FillMode = f; r.CullMode = c;
		r.DepthClipEnable = TRUE; r.MultisampleEnable = TRUE; r.AntialiasedLineEnable = aa; return r; };

	rasterizerStates[static_cast<int>(RasterizerState::SolidCullNone)] = std::make_unique<DX12RasterizerState>(mkR(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE));
	rasterizerStates[static_cast<int>(RasterizerState::SolidCullBack)] = std::make_unique<DX12RasterizerState>(mkR(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK));
	rasterizerStates[static_cast<int>(RasterizerState::WireCullNone)]  = std::make_unique<DX12RasterizerState>(mkR(D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_NONE, true));
	rasterizerStates[static_cast<int>(RasterizerState::WireCullBack)]  = std::make_unique<DX12RasterizerState>(mkR(D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_BACK, true));
}

RenderState::RenderState(ID3D11Device* device)
{
    // ==========================================
    // �T���v���X�e�[�g
    // ==========================================
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.MipLODBias = 0.0f; desc.MaxAnisotropy = 1; desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = -D3D11_FLOAT32_MAX; desc.MaxLOD = D3D11_FLOAT32_MAX;
        desc.BorderColor[0] = 1.0f; desc.BorderColor[1] = 1.0f; desc.BorderColor[2] = 1.0f; desc.BorderColor[3] = 1.0f;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP; desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerStates[static_cast<int>(SamplerState::PointWrap)] = std::make_unique<DX11Sampler>(device, desc);
    }
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.MipLODBias = 0.0f; desc.MaxAnisotropy = 1; desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = -D3D11_FLOAT32_MAX; desc.MaxLOD = D3D11_FLOAT32_MAX;
        desc.BorderColor[0] = 1.0f; desc.BorderColor[1] = 1.0f; desc.BorderColor[2] = 1.0f; desc.BorderColor[3] = 1.0f;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerStates[static_cast<int>(SamplerState::PointClamp)] = std::make_unique<DX11Sampler>(device, desc);
    }
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.MipLODBias = -1.0f; desc.MaxAnisotropy = 1; desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = -D3D11_FLOAT32_MAX; desc.MaxLOD = D3D11_FLOAT32_MAX;
        desc.BorderColor[0] = 1.0f; desc.BorderColor[1] = 1.0f; desc.BorderColor[2] = 1.0f; desc.BorderColor[3] = 1.0f;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP; desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerStates[static_cast<int>(SamplerState::LinearWrap)] = std::make_unique<DX11Sampler>(device, desc);
    }
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.MipLODBias = -1.0f; desc.MaxAnisotropy = 1; desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = -D3D11_FLOAT32_MAX; desc.MaxLOD = D3D11_FLOAT32_MAX;
        desc.BorderColor[0] = 1.0f; desc.BorderColor[1] = 1.0f; desc.BorderColor[2] = 1.0f; desc.BorderColor[3] = 1.0f;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerStates[static_cast<int>(SamplerState::LinearClamp)] = std::make_unique<DX11Sampler>(device, desc);
    }

    // ==========================================
    // �f�v�X�X�e�[�g
    // ==========================================
    auto createDepth = [&](D3D11_DEPTH_STENCIL_DESC desc, DepthState stateIdx) {
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
        HRESULT hr = device->CreateDepthStencilState(&desc, state.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        depthStencilStates[static_cast<int>(stateIdx)] = std::make_unique<DX11DepthStencilState>(state.Get());
        };

    { D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = true; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; createDepth(d, DepthState::TestAndWrite); }
    { D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = true; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; createDepth(d, DepthState::TestOnly); }
    { D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = true; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; d.DepthFunc = D3D11_COMPARISON_ALWAYS; createDepth(d, DepthState::WriteOnly); }
    { D3D11_DEPTH_STENCIL_DESC d = {}; d.DepthEnable = false; d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; d.DepthFunc = D3D11_COMPARISON_ALWAYS; createDepth(d, DepthState::NoTestNoWrite); }

    // ==========================================
    // �u�����h�X�e�[�g
    // ==========================================
    auto createBlend = [&](D3D11_BLEND_DESC desc, BlendState stateIdx) {
        Microsoft::WRL::ComPtr<ID3D11BlendState> state;
        HRESULT hr = device->CreateBlendState(&desc, state.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        blendStates[static_cast<int>(stateIdx)] = std::make_unique<DX11BlendState>(state.Get());
        };

    { D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = false; d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; createBlend(d, BlendState::Opaque); }
    { D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = true; d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; createBlend(d, BlendState::Transparency); }
    { D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = true; d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; d.RenderTarget[0].DestBlend = D3D11_BLEND_ONE; d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; createBlend(d, BlendState::Additive); }
    { D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = true; d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; d.RenderTarget[0].DestBlend = D3D11_BLEND_ONE; d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT; d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; createBlend(d, BlendState::Subtraction); }
    { D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = true; d.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO; d.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR; d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; createBlend(d, BlendState::Multiply); }
    { D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = true; d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; createBlend(d, BlendState::Alpha); }

    // ==========================================
    // ���X�^���C�U�X�e�[�g
    // ==========================================
    auto createRasterizer = [&](D3D11_RASTERIZER_DESC desc, RasterizerState stateIdx) {
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
        HRESULT hr = device->CreateRasterizerState(&desc, state.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        rasterizerStates[static_cast<int>(stateIdx)] = std::make_unique<DX11RasterizerState>(state.Get());
        };

    { D3D11_RASTERIZER_DESC d{}; d.DepthClipEnable = true; d.MultisampleEnable = true; d.FillMode = D3D11_FILL_SOLID; d.CullMode = D3D11_CULL_NONE; createRasterizer(d, RasterizerState::SolidCullNone); }
    { D3D11_RASTERIZER_DESC d{}; d.DepthClipEnable = true; d.MultisampleEnable = true; d.FillMode = D3D11_FILL_SOLID; d.CullMode = D3D11_CULL_BACK; createRasterizer(d, RasterizerState::SolidCullBack); }
    { D3D11_RASTERIZER_DESC d{}; d.DepthClipEnable = true; d.MultisampleEnable = true; d.FillMode = D3D11_FILL_WIREFRAME; d.CullMode = D3D11_CULL_NONE; d.AntialiasedLineEnable = true; createRasterizer(d, RasterizerState::WireCullNone); }
    { D3D11_RASTERIZER_DESC d{}; d.DepthClipEnable = true; d.MultisampleEnable = true; d.FillMode = D3D11_FILL_WIREFRAME; d.CullMode = D3D11_CULL_BACK; d.AntialiasedLineEnable = true; createRasterizer(d, RasterizerState::WireCullBack); }
}