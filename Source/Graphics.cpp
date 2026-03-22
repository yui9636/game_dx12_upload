#include "System/Misc.h"
#include "Graphics.h"
#include "ShaderClass/PhongShader.h"
#include "ShaderClass/PBRShader.h"
#include <srv.h>
#include <GpuResourceUtils.h>
#include "Camera/Camera.h"
#include "PostEffect.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX11/DX11PipelineState.h"
#include <RHI\DX11\DX11Texture.h>
#include <RHI\DX11\DX11ResourceFactory.h>
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12ResourceFactory.h"
#include "RHI/DX12/DX12Texture.h"
#include "Render/GlobalRootSignature.h"


using namespace Microsoft::WRL;

Graphics::~Graphics() = default;

void Graphics::Initialize(HWND hWnd, GraphicsAPI api)
{
	m_api = api;
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT w = rc.right - rc.left;
	UINT h = rc.bottom - rc.top;
	screenWidth = static_cast<float>(w);
	screenHeight = static_cast<float>(h);

	// ============================================================
	// DX12 蛻晄悄蛹悶ヱ繧ｹ
	// ============================================================
	if (api == GraphicsAPI::DX12) {
		m_dx12Device = std::make_unique<DX12Device>(hWnd, w, h);
		resourceFactory = std::make_unique<DX12ResourceFactory>(m_dx12Device.get());

		m_dx12BackBuffers[0] = std::make_shared<DX12Texture>(
			m_dx12Device.get(), m_dx12Device->GetBackBuffer(0), 0);
		m_dx12BackBuffers[1] = std::make_shared<DX12Texture>(
			m_dx12Device.get(), m_dx12Device->GetBackBuffer(1), 1);
		backBufferTexture = m_dx12BackBuffers[m_dx12Device->GetCurrentBackBufferIndex()];

		renderState = std::make_unique<RenderState>(m_dx12Device.get());

		auto* factory = resourceFactory.get();
		UINT renderW = static_cast<UINT>(w * m_renderScale);
		UINT renderH = static_cast<UINT>(h * m_renderScale);
		UINT halfW = renderW / 2;
		UINT halfH = renderH / 2;

		std::vector<TextureFormat> gBufFmt = {
			TextureFormat::R16G16B16A16_FLOAT, TextureFormat::R16G16B16A16_FLOAT,
			TextureFormat::R32G32B32A32_FLOAT, TextureFormat::R32G32_FLOAT };
		std::vector<TextureFormat> hdr = { TextureFormat::R16G16B16A16_FLOAT };
		std::vector<TextureFormat> ldr = { TextureFormat::RGBA8_UNORM };
		std::vector<TextureFormat> ao  = { TextureFormat::R8_UNORM };

		frameBuffers[static_cast<int>(FrameBufferId::GBuffer)]          = std::make_unique<FrameBuffer>(factory, renderW, renderH, gBufFmt);
		frameBuffers[static_cast<int>(FrameBufferId::Scene)]            = std::make_unique<FrameBuffer>(factory, renderW, renderH, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::PrevScene)]        = std::make_unique<FrameBuffer>(factory, renderW, renderH, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::PostProcess)]      = std::make_unique<FrameBuffer>(factory, renderW, renderH, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::Display)]          = std::make_unique<FrameBuffer>(factory, w, h, ldr);
		frameBuffers[static_cast<int>(FrameBufferId::Luminance)]        = std::make_unique<FrameBuffer>(factory, w, h, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::GTAO)]             = std::make_unique<FrameBuffer>(factory, renderW, renderH, ao);
		frameBuffers[static_cast<int>(FrameBufferId::SSGI)]             = std::make_unique<FrameBuffer>(factory, halfW, halfH, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::SSGIBlur)]         = std::make_unique<FrameBuffer>(factory, halfW, halfH, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::VolumetricFog)]    = std::make_unique<FrameBuffer>(factory, renderW / 2, renderH / 2, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::VolumetricFogBlur)]= std::make_unique<FrameBuffer>(factory, renderW / 2, renderH / 2, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::SSR)]              = std::make_unique<FrameBuffer>(factory, renderW / 2, renderH / 2, hdr);
		frameBuffers[static_cast<int>(FrameBufferId::SSRBlur)]          = std::make_unique<FrameBuffer>(factory, renderW / 2, renderH / 2, hdr);
		// GlobalRootSignature DX12蛻晄悄蛹・
		GlobalRootSignature::Instance().Initialize(m_dx12Device.get());

		// DX12蟇ｾ蠢懊し繝悶す繧ｹ繝・Β (IResourceFactory邨檎罰)
		gizmos = std::make_unique<Gizmos>(factory);
		shadowMap = std::make_unique<ShadowMap>(factory);
		modelRenderer = std::make_unique<ModelRenderer>(factory);
		// postEffect = nullptr (FSR2 DX11縺ｮ縺ｿ)
		// primitiveRenderer = nullptr (DX11逶ｴ謗･萓晏ｭ・
		return;
	}

	// ============================================================
	// DX11 蛻晄悄蛹悶ヱ繧ｹ (譌｢蟄・
	// ============================================================
	HRESULT hr = S_OK;

	// 1. ・ｽf・ｽo・ｽC・ｽX・ｽ・ｽ・ｽX・ｽ・ｽ・ｽb・ｽv・ｽ`・ｽF・ｽ[・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	{
		UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
		DXGI_SWAP_CHAIN_DESC sd{};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = w;
		sd.BufferDesc.Height = h;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
			featureLevels, 1, D3D11_SDK_VERSION, &sd,
			swapchain.GetAddressOf(), device.GetAddressOf(), nullptr, immediateContext.GetAddressOf());
	}

	// 2. ・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽ[・ｽ^・ｽ[・ｽQ・ｽb・ｽg・ｽr・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ・ｽ
	{
		ComPtr<ID3D11Texture2D> backBuffer;
		swapchain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
		device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());

		backBufferTexture = std::make_shared<DX11Texture>(renderTargetView.Get(), w, h);
	}

	// 3. ・ｽ・ｽ・ｽ[・ｽx・ｽX・ｽe・ｽ・ｽ・ｽV・ｽ・ｽ・ｽr・ｽ・ｽ・ｽ[・ｽ・ｽSRV・ｽﾌ撰ｿｽ・ｽ・ｽ (DoF・ｽﾎ会ｿｽ・ｽ・ｽ)
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = w;
		desc.Height = h;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		// ・ｽ・ｽ・ｽd・ｽv: R24G8_TYPELESS ・ｽﾉゑｿｽ・ｽ・ｽi・ｽ[・ｽx・ｽﾆゑｿｽ・ｽﾄゑｿｽ・ｽASRV・ｽﾆゑｿｽ・ｽﾄゑｿｽ・ｽﾇめゑｿｽ謔､・ｽﾉ）
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		// ・ｽ・ｽ・ｽd・ｽv: BIND_SHADER_RESOURCE ・ｽ・ｽﾇ会ｿｽ
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ComPtr<ID3D11Texture2D> depthStencil;
		device->CreateTexture2D(&desc, nullptr, depthStencil.GetAddressOf());

		// DSV・ｽ・ｬ (D24_S8)
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(depthStencil.Get(), &dsvDesc, depthStencilView.GetAddressOf());

		// ・ｽ・ｽSRV・ｽ・ｬ (R24_UNORM_X8) -> DoF・ｽV・ｽF・ｽ[・ｽ_・ｽ[・ｽﾅ深・ｽx・ｽ・ｽﾇむゑｿｽ・ｽ・ｽ
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(depthStencil.Get(), &srvDesc, depthStencilSRV.GetAddressOf());
	}

	// 4. ・ｽr・ｽ・ｽ・ｽ[・ｽ|・ｽ[・ｽg・ｽﾝ抵ｿｽ
	{
		D3D11_VIEWPORT vp{};
		vp.Width = static_cast<float>(w);
		vp.Height = static_cast<float>(h);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		immediateContext->RSSetViewports(1, &vp);
	}

	// 5. ・ｽe・ｽ・ｽ}・ｽl・ｽ[・ｽW・ｽ・ｽ・ｽE・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽ・ｽ・ｽﾌ撰ｿｽ・ｽ・ｽ
	renderState = std::make_unique<RenderState>(device.Get());
	primitiveRenderer = std::make_unique<PrimitiveRenderer>(device.Get());
	swordTrail = std::make_unique<SwordTrail>(device.Get());
	postEffect = std::make_unique<PostEffect>(device.Get());
	resourceFactory = std::make_unique<DX11ResourceFactory>(device.Get());
	gizmos = std::make_unique<Gizmos>(resourceFactory.get());
	shadowMap = std::make_unique<ShadowMap>(resourceFactory.get());
	modelRenderer = std::make_unique<ModelRenderer>(resourceFactory.get());

	// ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｽ
	//frameBuffers[static_cast<int>(FrameBufferId::Display)] = std::make_unique<FrameBuffer>(device.Get(), screenWidth, screenHeight);
	//frameBuffers[static_cast<int>(FrameBufferId::Scene)] = std::make_unique<FrameBuffer>(device.Get(), w, h);
	frameBuffers[static_cast<int>(FrameBufferId::Luminance)] = std::make_unique<FrameBuffer>(device.Get(), w, h);



	UINT renderW = static_cast<UINT>(w * m_renderScale);
	UINT renderH = static_cast<UINT>(h * m_renderScale);

	UINT halfW = renderW / 2;
	UINT halfH = renderH / 2;

	std::vector<DXGI_FORMAT> gBufferFormats = {
		DXGI_FORMAT_R16G16B16A16_FLOAT,   // Target 0: Albedo + Metallic
		DXGI_FORMAT_R16G16B16A16_FLOAT,   // Target 1: Normal + Roughness
		DXGI_FORMAT_R32G32B32A32_FLOAT,    // Target 2: World Position + Depth
		DXGI_FORMAT_R32G32_FLOAT
	};


	std::vector<DXGI_FORMAT> aoFormat = { DXGI_FORMAT_R8_UNORM };
    frameBuffers[static_cast<int>(FrameBufferId::GTAO)] = std::make_unique<FrameBuffer>(
        device.Get(),
        renderW, renderH, // FSR2・ｽp・ｽﾌ抵ｿｽ恣x・ｽT・ｽC・ｽY
        aoFormat          // ・ｽ・ｽ・ｽ・ｽ4・ｽ・ｽ・ｽ・ｽ・ｽﾜででス・ｽg・ｽb・ｽv・ｽI
    );


	std::vector<DXGI_FORMAT> ssgiFormat = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	frameBuffers[static_cast<int>(FrameBufferId::SSGI)] = std::make_unique<FrameBuffer>(
		device.Get(), halfW, halfH, ssgiFormat // ・ｽ・ｽ halfW, halfH ・ｽﾉ変更・ｽI
	);
	frameBuffers[static_cast<int>(FrameBufferId::SSGIBlur)] = std::make_unique<FrameBuffer>(
		device.Get(), halfW, halfH, ssgiFormat // ・ｽ・ｽ ・ｽﾇ会ｿｽ・ｽF・ｽu・ｽ・ｽ・ｽ[・ｽp
	);

	std::vector<DXGI_FORMAT> fogFormat = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	frameBuffers[static_cast<int>(FrameBufferId::VolumetricFog)] = std::make_unique<FrameBuffer>(
		device.Get(), renderW / 2, renderH / 2, fogFormat
	);
	frameBuffers[static_cast<int>(FrameBufferId::VolumetricFogBlur)] = std::make_unique<FrameBuffer>(
		device.Get(), renderW / 2, renderH / 2, fogFormat
	);

	std::vector<DXGI_FORMAT> ssrFormat = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	frameBuffers[static_cast<int>(FrameBufferId::SSR)] = std::make_unique<FrameBuffer>(
		device.Get(), renderW / 2, renderH / 2, ssrFormat
	);
	frameBuffers[static_cast<int>(FrameBufferId::SSRBlur)] = std::make_unique<FrameBuffer>(
		device.Get(), renderW / 2, renderH / 2, ssrFormat
	);
	

	std::vector<DXGI_FORMAT> hdrFormat = { DXGI_FORMAT_R16G16B16A16_FLOAT };

	frameBuffers[static_cast<int>(FrameBufferId::PostProcess)] = std::make_unique<FrameBuffer>(device.Get(), renderW, renderH, hdrFormat);
	frameBuffers[static_cast<int>(FrameBufferId::Display)] = std::make_unique<FrameBuffer>(device.Get(), w, h, hdrFormat);

	frameBuffers[static_cast<int>(FrameBufferId::GBuffer)] = std::make_unique<FrameBuffer>(device.Get(), renderW, renderH, gBufferFormats);
	frameBuffers[static_cast<int>(FrameBufferId::Scene)] = std::make_unique<FrameBuffer>(device.Get(), renderW, renderH);

	frameBuffers[static_cast<int>(FrameBufferId::PrevScene)] = std::make_unique<FrameBuffer>(device.Get(), renderW, renderH, hdrFormat);
}

static void DumpDRED(ID3D12Device* device) {
	ComPtr<ID3D12DeviceRemovedExtendedData> dred;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred)))) return;

	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs = {};
	if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs))) {
		FILE* f = nullptr;
		fopen_s(&f, "dx12_dred.log", "a");
		if (!f) return;
		const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode;
		while (node) {
			fprintf(f, "[DRED] CommandList: %S\n", node->pCommandListDebugNameW ? node->pCommandListDebugNameW : L"(unnamed)");
			fprintf(f, "[DRED] CommandQueue: %S\n", node->pCommandQueueDebugNameW ? node->pCommandQueueDebugNameW : L"(unnamed)");
			if (node->pLastBreadcrumbValue && node->pCommandHistory) {
				uint32_t lastCompleted = *node->pLastBreadcrumbValue;
				fprintf(f, "[DRED] LastCompleted=%u / Total=%u\n", lastCompleted, node->BreadcrumbCount);
				for (uint32_t i = 0; i < node->BreadcrumbCount && i < 64; ++i) {
					const char* marker = (i < lastCompleted) ? "DONE" : (i == lastCompleted ? ">>LAST>>" : "pending");
					fprintf(f, "[DRED]   [%u] op=%d %s\n", i, (int)node->pCommandHistory[i], marker);
				}
			}
			node = node->pNext;
		}
		fclose(f);
	}

	D3D12_DRED_PAGE_FAULT_OUTPUT pageFault = {};
	if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault))) {
		FILE* f = nullptr;
		fopen_s(&f, "dx12_dred.log", "a");
		if (f) {
			fprintf(f, "[DRED] PageFault VA=0x%llX\n", pageFault.PageFaultVA);
			fclose(f);
		}
	}
}

void Graphics::Present(UINT syncInterval)
{
	if (m_api == GraphicsAPI::DX12) {
		HRESULT hr = m_dx12Device->GetSwapChain()->Present(syncInterval, 0);
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
			HRESULT reason = m_dx12Device->GetDevice()->GetDeviceRemovedReason();
			char buf[256];
			sprintf_s(buf, "[Graphics::Present] DEVICE LOST! Present hr=0x%08X reason=0x%08X\n",
				(unsigned)hr, (unsigned)reason);
			OutputDebugStringA(buf);
			FILE* f = nullptr;
			fopen_s(&f, "dx12_device_lost.log", "a");
			if (f) { fprintf(f, "%s", buf); fclose(f); }
			DumpDRED(m_dx12Device->GetDevice());
		}
		m_dx12Device->MoveToNextFrame();
		backBufferTexture = m_dx12BackBuffers[m_dx12Device->GetCurrentBackBufferIndex()];
		return;
	}
	swapchain->Present(syncInterval, 0);
}

void Graphics::CopyFrameBuffer(FrameBuffer* source, FrameBuffer* destination)
{
	if (!source || !destination) return;

	if (m_api == GraphicsAPI::DX12) {
		// 證ｫ螳・ DX12縺ｧ縺ｯ繧ｹ繧ｭ繝・・ (蜑阪ヵ繝ｬ繝ｼ繝菫晏ｭ倥・蟆・擂蟇ｾ蠢・
		return;
	}

	ID3D11ShaderResourceView* sourceSRV = source->GetColorMap();
	ID3D11ShaderResourceView* destinationSRV = destination->GetColorMap();
	if (!sourceSRV || !destinationSRV) return;

	ComPtr<ID3D11Resource> srcRes;
	ComPtr<ID3D11Resource> dstRes;
	sourceSRV->GetResource(srcRes.GetAddressOf());
	destinationSRV->GetResource(dstRes.GetAddressOf());

	immediateContext->CopyResource(dstRes.Get(), srcRes.Get());
}

std::unique_ptr<IPipelineState> Graphics::CreatePipelineState(const PipelineStateDesc& desc)
{
	// DX11・ｽﾂ具ｿｽ・ｽﾅは、・ｽｯ趣ｿｽ・ｽ・ｽ・ｽDesc・ｽ・ｽ・ｽ・ｽ・ｽﾌまま保趣ｿｽ・ｽ・ｽ・ｽ・ｽDX11PipelineState・ｽｶ撰ｿｽ・ｽ・ｽ・ｽﾄ返ゑｿｽ
	return std::make_unique<DX11PipelineState>(desc);
}



