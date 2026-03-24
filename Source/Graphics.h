#pragma once

#include <d3d11.h>
#include <wrl.h>
#include "FrameBuffer.h"
#include <memory>
#include "RenderContext/RenderState.h"
#include <Gizmos.h>
#include "ShaderClass/Shader.h"
#include "ShadowMap.h"
#include "Model/ModelRenderer.h"
#include <mutex>
#include "PrimitiveRenderer.h"
#include "SwordTrail.h"
#include"PostEffect.h"
#include "RHI/IResourceFactory.h"
#include "RHI/GraphicsAPI.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Texture.h"

struct PipelineStateDesc;
class IPipelineState;
class ITexture;

// ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@ID
enum class FrameBufferId
{
	Display,
	Scene,      // ・ｽ・ｽ・ｽC・ｽ・ｽ・ｽV・ｽ[・ｽ・ｽ・ｽp
	PrevScene,
	GBuffer,
	Luminance,  // ・ｽu・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽp・ｽi・ｽP・ｽx・ｽ・ｽ・ｽo・ｽj
	GTAO,
	SSGI,
	SSGIBlur,
	VolumetricFog,      // ・ｽ・ｽ・ｽﾇ会ｿｽ: ・ｽt・ｽH・ｽO・ｽﾌ撰ｿｽ・ｽf・ｽ[・ｽ^
	VolumetricFogBlur,  // ・ｽ・ｽ・ｽﾇ会ｿｽ: ・ｽu・ｽ・ｽ・ｽ[・ｽﾏみの奇ｿｽ・ｽ轤ｩ・ｽﾈフ・ｽH・ｽO
	SSR,       // ・ｽ・ｽ・ｽﾇ会ｿｽ: ・ｽ・ｽ・ｽﾌ費ｿｽ・ｽﾋデ・ｽ[・ｽ^
	SSRBlur,   // ・ｽ・ｽ・ｽﾇ会ｿｽ: ・ｽu・ｽ・ｽ・ｽ[・ｽﾅ難ｿｽ・ｽ・ｽﾜゑｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾋデ・ｽ[・ｽ^
	PostProcess,
	EnumCount
};

class Graphics
{
private:
	Graphics() = default;
	~Graphics();

public:
	static Graphics& Instance()
	{
		static Graphics instance;
		return instance;
	}

	static bool IsShuttingDown();

	void Initialize(HWND hWnd, GraphicsAPI api = GraphicsAPI::DX11);
	void Present(UINT syncInterval);

	GraphicsAPI GetAPI() const { return m_api; }

	// DX11 accessors (nullptr when DX12)
	ID3D11Device* GetDevice() { return device.Get(); }
	ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }

	// DX12 accessor (nullptr when DX11)
	DX12Device* GetDX12Device() { return m_dx12Device.get(); }

	std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc);

	float GetScreenWidth() const { return screenWidth; }
	float GetScreenHeight() const { return screenHeight; }
	float GetRenderScale() const { return m_renderScale; }

	// ・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽ[・ｽ^・ｽ[・ｽQ・ｽb・ｽg・ｽr・ｽ・ｽ・ｽ[・ｽ謫ｾ
	ID3D11RenderTargetView* GetRenderTargetView() { return renderTargetView.Get(); }
	// ・ｽ[・ｽx・ｽX・ｽe・ｽ・ｽ・ｽV・ｽ・ｽ・ｽr・ｽ・ｽ・ｽ[・ｽ謫ｾ
	ID3D11DepthStencilView* GetDepthStencilView() { return depthStencilView.Get(); }

	// ・ｽ・ｽ・ｽﾇ会ｿｽ: ・ｽ[・ｽx・ｽX・ｽe・ｽ・ｽ・ｽV・ｽ・ｽSRV・ｽ謫ｾ (DoF・ｽp)
	ID3D11ShaderResourceView* GetDepthStencilSRV() { return depthStencilSRV.Get(); }

	ITexture* GetBackBufferTexture() const { return backBufferTexture.get(); }

	IResourceFactory* GetResourceFactory() const { return resourceFactory.get(); }

	// ・ｽe・ｽ・ｽT・ｽu・ｽV・ｽX・ｽe・ｽ・ｽ・ｽ謫ｾ

	RenderState* GetRenderState() { return renderState.get(); }
	Gizmos* GetGizmos() { return gizmos.get(); }
	PrimitiveRenderer* GetPrimitiveRenderer() { return primitiveRenderer.get(); }
	ShadowMap* GetShadowMap() { return shadowMap.get(); }
	ModelRenderer* GetModelRenderer() const { return modelRenderer.get(); }
	SwordTrail* GetSwordTrail() { return  swordTrail.get(); }
	PostEffect* GetPostEffect() const { return postEffect.get(); }
	std::mutex& GetMutex() { return mutex; }


	// ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｽ
	FrameBuffer* GetFrameBuffer(FrameBufferId id) const { return frameBuffers[static_cast<int>(id)].get(); }
	void CopyFrameBuffer(FrameBuffer* source, FrameBuffer* destination);

private:
	Microsoft::WRL::ComPtr<ID3D11Device>			device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		immediateContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>			swapchain;

	// ・ｽo・ｽb・ｽN・ｽo・ｽb・ｽt・ｽ@・ｽﾖ連
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	depthStencilView;
	// ・ｽ・ｽ・ｽﾇ会ｿｽ: ・ｽ[・ｽx・ｽﾇみ搾ｿｽ・ｽﾝ用SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthStencilSRV;


	std::unique_ptr<FrameBuffer>			frameBuffers[static_cast<int>(FrameBufferId::EnumCount)];
	std::unique_ptr<RenderState>			renderState;
	std::unique_ptr<Gizmos>					gizmos;
	std::unique_ptr<ShadowMap>				shadowMap;
	std::unique_ptr<ModelRenderer>			modelRenderer;
	std::unique_ptr<PrimitiveRenderer>		primitiveRenderer;
	std::unique_ptr<SwordTrail>				swordTrail;
	std::unique_ptr<PostEffect>				postEffect;

	std::shared_ptr<ITexture> backBufferTexture;
	std::unique_ptr<IResourceFactory> resourceFactory;

	// DX12 backend
	std::unique_ptr<DX12Device> m_dx12Device;
	std::shared_ptr<DX12Texture> m_dx12BackBuffers[DX12Device::FRAME_COUNT];
	GraphicsAPI m_api = GraphicsAPI::DX11;

	std::mutex mutex;
	float screenWidth;
	float screenHeight;
	float m_renderScale = 0.67f;
};
