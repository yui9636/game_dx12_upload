#pragma once

#include <d3d11.h>
#include <wrl.h>
#include "Render/FrameBuffer.h"
#include <memory>
#include "RenderContext/RenderState.h"
#include "Render/Gizmos.h"
#include "ShaderClass/Shader.h"
#include "Render/ShadowMap.h"
#include "Model/ModelRenderer.h"
#include <mutex>
#include "Render/PrimitiveRenderer.h"
#include"PostEffect/PostEffect.h"
#include "RHI/IResourceFactory.h"
#include "RHI/GraphicsAPI.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Texture.h"

struct PipelineStateDesc;
class IPipelineState;
class ITexture;

enum class FrameBufferId
{
	Display,             // 最終的に画面へ出す back buffer 相当。
	EditorDisplay,       // editor viewport 用の表示先。
	Scene,               // game 実行側の HDR scene color。
	PrevScene,           // TAA / motion 系で参照する前フレーム scene。
	EditorScene,         // editor viewport 側の HDR scene color。
	EditorPrevScene,     // editor viewport 側の前フレーム scene。
	GBuffer,             // deferred rendering 用の複数 MRT。
	Luminance,           // game 側の輝度・postprocess 入力。
	EditorLuminance,     // editor 側の輝度・postprocess 入力。
	GTAO,                // GTAO の出力。
	SSGI,                // SSGI の一次出力。
	SSGIBlur,            // SSGI blur 後の出力。
	VolumetricFog,       // volumetric fog の一次出力。
	VolumetricFogBlur,   // volumetric fog blur 後の出力。
	SSR,                 // screen space reflection の一次出力。
	SSRBlur,             // SSR blur 後の出力。
	PostProcess,         // game 側の最終 postprocess 出力。
	EditorPostProcess,   // editor 側の最終 postprocess 出力。
	EnumCount            // frameBuffers 配列サイズ。
};

class Graphics
{
private:
	Graphics() = default;
	~Graphics();

public:
	// singleton として renderer 全体の device / swapchain / 共通 renderer を管理する。
	static Graphics& Instance()
	{
		static Graphics instance;
		return instance;
	}

	static bool IsShuttingDown();

	// 選択された GraphicsAPI で device、swapchain、共通 render resource を初期化する。
	void Initialize(HWND hWnd, GraphicsAPI api = GraphicsAPI::DX11);
	// window resize 時に swapchain と各 FrameBuffer を作り直す。
	void OnResize(uint32_t width, uint32_t height);
	// back buffer を present し、DX12 では frame index と deferred release も進める。
	void Present(UINT syncInterval);
	HWND GetWindowHandle() const { return m_windowHandle; }

	GraphicsAPI GetAPI() const { return m_api; }

	// DX11 用アクセサ（DX12 時は nullptr）
	ID3D11Device* GetDevice() { return device.Get(); }
	ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }

	// DX12 用アクセサ（DX11 時は nullptr）
	DX12Device* GetDX12Device() { return m_dx12Device.get(); }

	// 現在の backend に対応した pipeline state wrapper を作る。
	std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc);

	float GetScreenWidth() const { return screenWidth; }
	float GetScreenHeight() const { return screenHeight; }
	float GetRenderScale() const { return m_renderScale; }

	ID3D11RenderTargetView* GetRenderTargetView() { return renderTargetView.Get(); }
	ID3D11DepthStencilView* GetDepthStencilView() { return depthStencilView.Get(); }

	ID3D11ShaderResourceView* GetDepthStencilSRV() { return depthStencilSRV.Get(); }

	ITexture* GetBackBufferTexture() const { return backBufferTexture.get(); }

	IResourceFactory* GetResourceFactory() const { return resourceFactory.get(); }


	RenderState* GetRenderState() { return renderState.get(); }
	Gizmos* GetGizmos() { return gizmos.get(); }
	PrimitiveRenderer* GetPrimitiveRenderer() { return primitiveRenderer.get(); }
	ShadowMap* GetShadowMap() { return shadowMap.get(); }
	ModelRenderer* GetModelRenderer() const { return modelRenderer.get(); }
	PostEffect* GetPostEffect() const { return postEffect.get(); }
	std::mutex& GetMutex() { return mutex; }


	FrameBuffer* GetFrameBuffer(FrameBufferId id) const { return frameBuffers[static_cast<int>(id)].get(); }
	// source の color texture を destination へコピーする。
	void CopyFrameBuffer(FrameBuffer* source, FrameBuffer* destination);

private:
	Microsoft::WRL::ComPtr<ID3D11Device>			device;           // DX11 device。DX12 使用時は空。
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		immediateContext; // DX11 immediate context。DX12 使用時は空。
	Microsoft::WRL::ComPtr<IDXGISwapChain>			swapchain;        // DX11 swapchain。

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	renderTargetView; // DX11 back buffer RTV。
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	depthStencilView; // DX11 main depth DSV。
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthStencilSRV; // DX11 depth SRV。


	std::unique_ptr<FrameBuffer>			frameBuffers[static_cast<int>(FrameBufferId::EnumCount)]; // 各 render pass 用の render target 群。
	std::unique_ptr<RenderState>			renderState;       // camera / light / viewport などの共通描画状態。
	std::unique_ptr<Gizmos>					gizmos;            // debug shape renderer。
	std::unique_ptr<ShadowMap>				shadowMap;         // cascade shadow map renderer。
	std::unique_ptr<ModelRenderer>			modelRenderer;     // model draw queue と描画実行。
	std::unique_ptr<PrimitiveRenderer>		primitiveRenderer; // DX11 line primitive renderer。
	std::unique_ptr<PostEffect>				postEffect;        // postprocess 管理。

	std::shared_ptr<ITexture> backBufferTexture;       // 現在 frame の back buffer を RHI texture として参照する。
	std::unique_ptr<IResourceFactory> resourceFactory; // backend ごとの texture / buffer / shader factory。

	// DX12 バックエンド
	std::unique_ptr<DX12Device> m_dx12Device; // DX12 device と swapchain 管理。
	std::shared_ptr<DX12Texture> m_dx12BackBuffers[DX12Device::FRAME_COUNT]; // swapchain back buffer wrapper。
	GraphicsAPI m_api = GraphicsAPI::DX11; // 現在使用中の backend。

	std::mutex mutex;              // render resource 共有時の簡易同期。
	float screenWidth;             // window の pixel 幅。
	float screenHeight;            // window の pixel 高さ。
	float m_renderScale = 0.67f;   // 内部 render target の解像度倍率。
	HWND m_windowHandle = nullptr; // swapchain と ImGui が使う window handle。
};
