#pragma once

#include <wrl.h>
#include <d3d11.h>
#include <memory>

class ISampler;
class IDepthStencilState;
class IBlendState;
class IRasterizerState;


enum class SamplerState
{
	PointWrap,
	PointClamp,
	LinearWrap,
	LinearClamp,

	EnumCount
};

enum class DepthState
{
	TestAndWrite,
	TestOnly,
	WriteOnly,
	NoTestNoWrite,

	EnumCount
};

enum class BlendState
{
	Opaque,
	Transparency,
	Additive,
	Subtraction,
	Multiply,
	Alpha,

	EnumCount
};

enum class RasterizerState
{
	SolidCullNone,
	SolidCullBack,
	WireCullNone,
	WireCullBack,

	EnumCount
};

class DX12Device;

class RenderState
{
public:
	RenderState(ID3D11Device* device);
	RenderState(DX12Device* device);
	~RenderState();

	ISampler* GetSamplerState(SamplerState state) const
	{
		return samplerStates[static_cast<int>(state)].get();
	}

	IDepthStencilState* GetDepthStencilState(DepthState state) const {
		return depthStencilStates[static_cast<int>(state)].get();
	}

	IBlendState* GetBlendState(BlendState state) const {
		return blendStates[static_cast<int>(state)].get();
	}

	IRasterizerState* GetRasterizerState(RasterizerState state) const {
		return rasterizerStates[static_cast<int>(state)].get();
	}

private:
	std::unique_ptr<ISampler>                       samplerStates[static_cast<int>(SamplerState::EnumCount)];
	std::unique_ptr<IDepthStencilState> depthStencilStates[static_cast<int>(DepthState::EnumCount)];
	std::unique_ptr<IBlendState>        blendStates[static_cast<int>(BlendState::EnumCount)];
	std::unique_ptr<IRasterizerState>   rasterizerStates[static_cast<int>(RasterizerState::EnumCount)];

};
