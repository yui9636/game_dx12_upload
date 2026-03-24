//#include "PostEffect.h"
//#include "GpuResourceUtils.h"
//#include "imgui.h"
//#include <Graphics.h>
//#include "RHI/ICommandList.h"
//
//#pragma comment(lib, "dxguid.lib")
//
//PostEffect::PostEffect(ID3D11Device* device)
//{
//	GpuResourceUtils::LoadVertexShader(
//		device,
//		"Data/Shader/FullScreenQuadVs.cso",
//		nullptr, 0,
//		nullptr,
//		fullscreenQuadVS.GetAddressOf());
//	GpuResourceUtils::LoadPixelShader(
//		device,
//		"Data/Shader/LuminanceExtractionPS.cso",
//		luminanceExtractionPS.GetAddressOf());
//	GpuResourceUtils::CreateConstantBuffer(
//		device,
//		sizeof(CbPostEffect),
//		constantBuffer.GetAddressOf());
//	GpuResourceUtils::LoadPixelShader(
//		device,
//		"Data/Shader/BloomPS.cso",
//		bloomPS.GetAddressOf());
//	GpuResourceUtils::LoadPixelShader(
//		device,
//		"Data/Shader/ColorFilterPS.cso",
//		colorFilterPS.GetAddressOf());
//	GpuResourceUtils::LoadPixelShader(
//		device,
//		"Data/Shader/DepthOfFieldPS.cso",
//		dofPS.GetAddressOf());
//
//
//	const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX11();
//	void* scratchBuffer = malloc(scratchBufferSize);
//
//	ffxFsr2GetInterfaceDX11(&m_fsr2Interface, device, scratchBuffer, scratchBufferSize);
//
//	FfxFsr2ContextDescription contextDesc = {};
//
//	contextDesc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_AUTO_EXPOSURE;
//
//	float renderScale = 0.67f;
//	uint32_t screenW = static_cast<uint32_t>(Graphics::Instance().GetScreenWidth());
//	uint32_t screenH = static_cast<uint32_t>(Graphics::Instance().GetScreenHeight());
//
//	contextDesc.maxRenderSize.width = static_cast<uint32_t>(screenW * renderScale);
//	contextDesc.maxRenderSize.height = static_cast<uint32_t>(screenH * renderScale);
//	contextDesc.displaySize.width = screenW;
//	contextDesc.displaySize.height = screenH;
//
//	contextDesc.callbacks = m_fsr2Interface;
//	contextDesc.device = ffxGetDeviceDX11(device);
//
//	FfxErrorCode errorCode = ffxFsr2ContextCreate(&m_fsr2Context, &contextDesc);
//
//	if (errorCode == FFX_OK) {
//		m_fsr2Initialized = true;
//	}
//	else {
//		m_fsr2Initialized = false;
//	}
//
//
//}
//
//PostEffect::~PostEffect() {
//	if (m_fsr2Initialized) {
//		ffxFsr2ContextDestroy(&m_fsr2Context);
//		free(m_fsr2Interface.scratchBuffer);
//	}
//}
//
//
//void PostEffect::Bloom(const RenderContext& rc, ID3D11ShaderResourceView* colorMap, ID3D11ShaderResourceView* luminanceMap)
//{
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	dc->VSSetShader(fullscreenQuadVS.Get(), 0, 0);
//	dc->PSSetShader(bloomPS.Get(), 0, 0);
//	ID3D11ShaderResourceView* srvs[] = { colorMap,luminanceMap };
//	dc->PSSetShaderResources(0, _countof(srvs), srvs);
//	dc->Draw(4, 0);
//}
//
//void PostEffect::LuminanceExtraction(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
//{
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
//	luminanceFB->SetRenderTarget(dc, nullptr);
//
//	dc->VSSetShader(fullscreenQuadVS.Get(), 0, 0);
//	dc->PSSetShader(luminanceExtractionPS.Get(), 0, 0);
//
//	// ========================================================
//	dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
//
//	ID3D11SamplerState* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
//	dc->PSSetSamplers(0, 1, &sampler);
//	// ========================================================
//
//	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//	dc->IASetInputLayout(nullptr);
//
//	ID3D11ShaderResourceView* srvs[] = { colorMap };
//	dc->PSSetShaderResources(0, _countof(srvs), srvs);
//
//	dc->Draw(4, 0);
//
//	ID3D11ShaderResourceView* nullSRVs[] = { nullptr };
//	dc->PSSetShaderResources(0, 1, nullSRVs);
//}
//
//void PostEffect::Process(const RenderContext& rc, FrameBuffer* src, FrameBuffer* dst)
//{
//	ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//	cbPostEffect.time = rc.time;
//	cbPostEffect.luminanceExtractionLowerEdge = rc.bloomData.luminanceLowerEdge;
//	cbPostEffect.luminanceExtractionHigherEdge = rc.bloomData.luminanceHigherEdge;
//	cbPostEffect.bloomIntensity = rc.bloomData.bloomIntensity;
//	cbPostEffect.gaussianSigma = rc.bloomData.gaussianSigma;
//	cbPostEffect.exposure = rc.colorFilterData.exposure;
//	cbPostEffect.monoBlend = rc.colorFilterData.monoBlend;
//	cbPostEffect.hueShift = rc.colorFilterData.hueShift;
//	cbPostEffect.flashAmount = rc.colorFilterData.flashAmount;
//	cbPostEffect.vignetteAmount = rc.colorFilterData.vignetteAmount;
//	cbPostEffect.focusDistance = rc.dofData.focusDistance;
//	cbPostEffect.focusRange = rc.dofData.focusRange;
//	cbPostEffect.bokehRadius = rc.dofData.enable ? rc.dofData.bokehRadius : 0.0f;
//
//	cbPostEffect.motionBlurIntensity = rc.motionBlurData.intensity;
//	cbPostEffect.motionBlurSamples = rc.motionBlurData.samples;
//
//	dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cbPostEffect, 0, 0);
//
//	FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
//	luminanceFB->Clear(dc, 0, 0, 0, 0);
//	LuminanceExtraction(rc, src->GetColorMap());
//
//
//	
//	FrameBuffer* workFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::PostProcess);
//	workFB->SetRenderTarget(dc, nullptr);
//
//	dc->VSSetShader(fullscreenQuadVS.Get(), nullptr, 0);
//	dc->PSSetShader(bloomPS.Get(), nullptr, 0);
//	dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
//	dc->IASetInputLayout(nullptr);
//
//	// =========================================================
//	// =========================================================
//	FrameBuffer* gBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer);
//	ID3D11ShaderResourceView* velocitySRV = gBuffer ? gBuffer->GetColorMap(3) : nullptr;
//
//	ID3D11ShaderResourceView* srvs[] = {
//		src->GetColorMap(),         // t0: colorMap
//		luminanceFB->GetColorMap(), // t1: luminanceMap
//	};
//	dc->PSSetShaderResources(0, _countof(srvs), srvs);
//
//	ID3D11SamplerState* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
//	dc->PSSetSamplers(0, 1, &sampler);
//
//	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//	dc->Draw(4, 0);
//
//	ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr };
//	dc->PSSetShaderResources(0, _countof(nullSRVs), nullSRVs);
//
//
//
//
//	ID3D11RenderTargetView* nullRTVs[] = { nullptr };
//	dc->OMSetRenderTargets(1, nullRTVs, nullptr);
//
//	if (m_fsr2Initialized)
//	{
//		FfxFsr2DispatchDescription dispatchDesc = {};
//		dispatchDesc.commandList = (FfxCommandList)dc;
//
//		ID3D11Resource* resColor = nullptr;
//		ID3D11Resource* resDepth = nullptr;
//		ID3D11Resource* resVelocity = nullptr;
//		ID3D11Resource* resOutput = nullptr;
//
//		workFB->GetColorMap()->GetResource(&resColor);
//		dispatchDesc.color = ffxGetResourceDX11(&m_fsr2Context, resColor, L"FSR2_InputColor", FFX_RESOURCE_STATE_COMPUTE_READ);
//
//		FrameBuffer* gBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer);
//		if (gBuffer && gBuffer->GetDepthMap()) {
//			gBuffer->GetDepthMap()->GetResource(&resDepth);
//			dispatchDesc.depth = ffxGetResourceDX11(&m_fsr2Context, resDepth, L"FSR2_InputDepth", FFX_RESOURCE_STATE_COMPUTE_READ);
//		}
//
//		if (gBuffer && gBuffer->GetColorMap(3)) {
//			gBuffer->GetColorMap(3)->GetResource(&resVelocity);
//			dispatchDesc.motionVectors = ffxGetResourceDX11(&m_fsr2Context, resVelocity, L"FSR2_InputVelocity", FFX_RESOURCE_STATE_COMPUTE_READ);
//		}
//
//		dst->GetRenderTargetView()->GetResource(&resOutput);
//		dispatchDesc.output = ffxGetResourceDX11(&m_fsr2Context, resOutput, L"FSR2_Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
//
//		// =========================================================
//		// =========================================================
//
//		
//
//		float renderScale = 0.67f;
//		float renderW = (float)(uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
//		float renderH = (float)(uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);
//
//
//		dispatchDesc.motionVectorScale.x = renderW;
//		dispatchDesc.motionVectorScale.y = renderH;
//
//		dispatchDesc.jitterOffset.x = rc.jitterOffset.x;
//		dispatchDesc.jitterOffset.y = rc.jitterOffset.y;
//		dispatchDesc.reset = false;
//		dispatchDesc.enableSharpening = true;
//		dispatchDesc.sharpness = 0.5f;
//
//		static float lastTime = 0.0f;
//		float deltaTime = (rc.time - lastTime) * 1000.0f;
//		if (deltaTime <= 0.0f) deltaTime = 16.6f;
//		dispatchDesc.frameTimeDelta = deltaTime;
//		lastTime = rc.time;
//
//		dispatchDesc.renderSize.width = static_cast<uint32_t>(renderW);
//		dispatchDesc.renderSize.height = static_cast<uint32_t>(renderH);
//		dispatchDesc.preExposure = 1.0f;
//
//		dispatchDesc.cameraNear = (rc.nearZ > 0.0f) ? rc.nearZ : 0.1f;
//		dispatchDesc.cameraFar = (rc.farZ > 0.0f) ? rc.farZ : 1000.0f;
//		dispatchDesc.cameraFovAngleVertical = (rc.fovY > 0.0f) ? rc.fovY : 1.047f;
//
//		ffxFsr2ContextDispatch(&m_fsr2Context, &dispatchDesc);
//
//		if (resColor) resColor->Release();
//		if (resDepth) resDepth->Release();
//		if (resVelocity) resVelocity->Release();
//		if (resOutput) resOutput->Release();
//	}
//}
//
//
//void PostEffect::DrawDebugGUI()
//{
//	ImGui::DragFloat("LuminanceLowerEdge", &cbPostEffect.luminanceExtractionLowerEdge, 0.01f, 0, 1.0f);
//	ImGui::DragFloat("LuminanceHigherEdge", &cbPostEffect.luminanceExtractionHigherEdge, 0.01f, 0, 1.0f);
//	ImGui::DragFloat("GaussianSigma", &cbPostEffect.gaussianSigma, 0.01f, 0, 10.0f);
//	ImGui::DragFloat("BloomIntensity", &cbPostEffect.bloomIntensity, 0.1f, 0, 10.0f);
//	ImGui::DragFloat("monoBlend", &cbPostEffect.monoBlend, 0.01f, 0.0f, 1.0f);
//	ImGui::DragFloat("hueShift", &cbPostEffect.hueShift, 0.0f, 0.0f, 1.0f);
//	ImGui::DragFloat("flashAmount", &cbPostEffect.flashAmount, 0.0f, 0.0f, 1.0f);
//	ImGui::DragFloat("vignetteAmount", &cbPostEffect.vignetteAmount, 0.0f, 0.0f, 1.0f);
//}
//
//#include "PostEffect.h"
//#include "GpuResourceUtils.h"
//#include "imgui.h"
//#include <Graphics.h>
//#include "RHI/ICommandList.h"
//#include "RHI/IShader.h"
//#include "RHI/DX11/DX11Shader.h"
//#include "RHI/IBuffer.h"
//#include "RHI/DX11/DX11Buffer.h"
//#include "RHI/ISampler.h"
//#include "RHI/ITexture.h"
//
//#pragma comment(lib, "dxguid.lib")
//
//PostEffect::PostEffect(ID3D11Device* device)
//{
//    fullscreenQuadVS = std::make_unique<DX11Shader>(device, ShaderType::Vertex, "Data/Shader/FullScreenQuadVs.cso");
//    luminanceExtractionPS = std::make_unique<DX11Shader>(device, ShaderType::Pixel, "Data/Shader/LuminanceExtractionPS.cso");
//    bloomPS = std::make_unique<DX11Shader>(device, ShaderType::Pixel, "Data/Shader/BloomPS.cso");
//    colorFilterPS = std::make_unique<DX11Shader>(device, ShaderType::Pixel, "Data/Shader/ColorFilterPS.cso");
//    dofPS = std::make_unique<DX11Shader>(device, ShaderType::Pixel, "Data/Shader/DepthOfFieldPS.cso");
//
//    constantBuffer = std::make_unique<DX11Buffer>(device, sizeof(CbPostEffect), BufferType::Constant);
//
//    const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX11();
//    void* scratchBuffer = malloc(scratchBufferSize);
//
//    ffxFsr2GetInterfaceDX11(&m_fsr2Interface, device, scratchBuffer, scratchBufferSize);
//
//    FfxFsr2ContextDescription contextDesc = {};
//    contextDesc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_AUTO_EXPOSURE;
//
//    float renderScale = 0.67f;
//    uint32_t screenW = static_cast<uint32_t>(Graphics::Instance().GetScreenWidth());
//    uint32_t screenH = static_cast<uint32_t>(Graphics::Instance().GetScreenHeight());
//
//    contextDesc.maxRenderSize.width = static_cast<uint32_t>(screenW * renderScale);
//    contextDesc.maxRenderSize.height = static_cast<uint32_t>(screenH * renderScale);
//    contextDesc.displaySize.width = screenW;
//    contextDesc.displaySize.height = screenH;
//
//    contextDesc.callbacks = m_fsr2Interface;
//    contextDesc.device = ffxGetDeviceDX11(device);
//
//    FfxErrorCode errorCode = ffxFsr2ContextCreate(&m_fsr2Context, &contextDesc);
//    m_fsr2Initialized = (errorCode == FFX_OK);
//}
//
//PostEffect::~PostEffect() {
//    if (m_fsr2Initialized) {
//        ffxFsr2ContextDestroy(&m_fsr2Context);
//        free(m_fsr2Interface.scratchBuffer);
//    }
//}
//
//void PostEffect::Bloom(const RenderContext& rc, ITexture* colorMap, ITexture* luminanceMap)
//{
//    rc.commandList->VSSetShader(fullscreenQuadVS.get());
//    rc.commandList->PSSetShader(bloomPS.get());
//
//    ITexture* srvs[] = { colorMap, luminanceMap };
//    rc.commandList->PSSetTextures(0, 2, srvs);
//
//    rc.commandList->Draw(4, 0);
//}
//
//void PostEffect::LuminanceExtraction(const RenderContext& rc, ITexture* colorMap)
//{
//    FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
//    luminanceFB->SetRenderTarget(rc.commandList, nullptr);
//
//    rc.commandList->VSSetShader(fullscreenQuadVS.get());
//    rc.commandList->PSSetShader(luminanceExtractionPS.get());
//    rc.commandList->PSSetConstantBuffer(0, constantBuffer.get());
//
//    ISampler* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
//    rc.commandList->PSSetSampler(0, sampler);
//
//    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
//    rc.commandList->SetInputLayout(nullptr);
//
//    rc.commandList->PSSetTexture(0, colorMap);
//    rc.commandList->Draw(4, 0);
//    rc.commandList->PSSetTexture(0, nullptr);
//}
//
//void PostEffect::Process(const RenderContext& rc, FrameBuffer* src, FrameBuffer* dst)
//{
//    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//
//    cbPostEffect.time = rc.time;
//    cbPostEffect.luminanceExtractionLowerEdge = rc.bloomData.luminanceLowerEdge;
//    cbPostEffect.luminanceExtractionHigherEdge = rc.bloomData.luminanceHigherEdge;
//    cbPostEffect.bloomIntensity = rc.bloomData.bloomIntensity;
//    cbPostEffect.gaussianSigma = rc.bloomData.gaussianSigma;
//    cbPostEffect.exposure = rc.colorFilterData.exposure;
//    cbPostEffect.monoBlend = rc.colorFilterData.monoBlend;
//    cbPostEffect.hueShift = rc.colorFilterData.hueShift;
//    cbPostEffect.flashAmount = rc.colorFilterData.flashAmount;
//    cbPostEffect.vignetteAmount = rc.colorFilterData.vignetteAmount;
//    cbPostEffect.focusDistance = rc.dofData.focusDistance;
//    cbPostEffect.focusRange = rc.dofData.focusRange;
//    cbPostEffect.bokehRadius = rc.dofData.enable ? rc.dofData.bokehRadius : 0.0f;
//    cbPostEffect.motionBlurIntensity = rc.motionBlurData.intensity;
//    cbPostEffect.motionBlurSamples = rc.motionBlurData.samples;
//
//    rc.commandList->UpdateBuffer(constantBuffer.get(), &cbPostEffect, sizeof(cbPostEffect));
//
//    FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
//    luminanceFB->Clear(rc.commandList, 0, 0, 0, 0);
//    LuminanceExtraction(rc, src->GetColorTexture(0));
//
//    FrameBuffer* workFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::PostProcess);
//    workFB->SetRenderTarget(rc.commandList, nullptr);
//
//    rc.commandList->VSSetShader(fullscreenQuadVS.get());
//    rc.commandList->PSSetShader(bloomPS.get());
//    rc.commandList->PSSetConstantBuffer(0, constantBuffer.get());
//
//    rc.commandList->SetInputLayout(nullptr);
//
//    FrameBuffer* gBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer);
//    ITexture* velocityTex = gBuffer ? gBuffer->GetColorTexture(3) : nullptr;
//
//    rc.commandList->PSSetTexture(0, src->GetColorTexture(0));
//    rc.commandList->PSSetTexture(1, luminanceFB->GetColorTexture(0));
//    rc.commandList->PSSetTexture(3, velocityTex);
//
//    if (rc.sceneDepthSRV) {
//        dc->PSSetShaderResources(2, 1, &rc.sceneDepthSRV);
//    }
//    else {
//        ID3D11ShaderResourceView* nullSrv = nullptr;
//        dc->PSSetShaderResources(2, 1, &nullSrv);
//    }
//
//    ISampler* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
//    rc.commandList->PSSetSampler(0, sampler);
//
//    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
//    rc.commandList->Draw(4, 0);
//
//    rc.commandList->PSSetTexture(0, nullptr);
//    rc.commandList->PSSetTexture(1, nullptr);
//    rc.commandList->PSSetTexture(3, nullptr);
//    rc.commandList->PSSetTexture(2, nullptr);
//
//    rc.commandList->SetRenderTarget(nullptr, nullptr);
//
//    if (m_fsr2Initialized)
//    {
//        FfxFsr2DispatchDescription dispatchDesc = {};
//        dispatchDesc.commandList = (FfxCommandList)dc;
//
//        ID3D11Resource* resColor = nullptr;
//        ID3D11Resource* resDepth = nullptr;
//        ID3D11Resource* resVelocity = nullptr;
//        ID3D11Resource* resOutput = nullptr;
//
//        workFB->GetColorMap()->GetResource(&resColor);
//        dispatchDesc.color = ffxGetResourceDX11(&m_fsr2Context, resColor, L"FSR2_InputColor", FFX_RESOURCE_STATE_COMPUTE_READ);
//
//        if (gBuffer && gBuffer->GetDepthMap()) {
//            gBuffer->GetDepthMap()->GetResource(&resDepth);
//            dispatchDesc.depth = ffxGetResourceDX11(&m_fsr2Context, resDepth, L"FSR2_InputDepth", FFX_RESOURCE_STATE_COMPUTE_READ);
//        }
//
//        if (gBuffer && gBuffer->GetColorMap(3)) {
//            gBuffer->GetColorMap(3)->GetResource(&resVelocity);
//            dispatchDesc.motionVectors = ffxGetResourceDX11(&m_fsr2Context, resVelocity, L"FSR2_InputVelocity", FFX_RESOURCE_STATE_COMPUTE_READ);
//        }
//
//        dst->GetRenderTargetView()->GetResource(&resOutput);
//        dispatchDesc.output = ffxGetResourceDX11(&m_fsr2Context, resOutput, L"FSR2_Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
//
//        float rW = (float)(uint32_t)(Graphics::Instance().GetScreenWidth() * 0.67f);
//        float rH = (float)(uint32_t)(Graphics::Instance().GetScreenHeight() * 0.67f);
//
//        dispatchDesc.motionVectorScale.x = rW;
//        dispatchDesc.motionVectorScale.y = rH;
//        dispatchDesc.jitterOffset.x = rc.jitterOffset.x;
//        dispatchDesc.jitterOffset.y = rc.jitterOffset.y;
//        dispatchDesc.reset = false;
//        dispatchDesc.enableSharpening = true;
//        dispatchDesc.sharpness = 0.5f;
//
//        static float lastTime = 0.0f;
//        float dT = (rc.time - lastTime) * 1000.0f;
//        if (dT <= 0.0f) dT = 16.6f;
//        dispatchDesc.frameTimeDelta = dT;
//        lastTime = rc.time;
//
//        dispatchDesc.renderSize.width = static_cast<uint32_t>(rW);
//        dispatchDesc.renderSize.height = static_cast<uint32_t>(rH);
//        dispatchDesc.preExposure = 1.0f;
//        dispatchDesc.cameraNear = (rc.nearZ > 0.0f) ? rc.nearZ : 0.1f;
//        dispatchDesc.cameraFar = (rc.farZ > 0.0f) ? rc.farZ : 1000.0f;
//        dispatchDesc.cameraFovAngleVertical = (rc.fovY > 0.0f) ? rc.fovY : 1.047f;
//
//        ffxFsr2ContextDispatch(&m_fsr2Context, &dispatchDesc);
//
//        if (resColor) resColor->Release();
//        if (resDepth) resDepth->Release();
//        if (resVelocity) resVelocity->Release();
//        if (resOutput) resOutput->Release();
//    }
//}
//
//
//
//
#include "PostEffect.h"
#include "GpuResourceUtils.h"
#include "imgui.h"
#include <Graphics.h>

#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IBuffer.h"
#include "RHI/IPipelineState.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Buffer.h"
#include "RHI/DX11/DX11Texture.h"

#pragma comment(lib, "dxguid.lib")

PostEffect::PostEffect(ID3D11Device* device)
{
    fullscreenQuadVS = std::make_unique<DX11Shader>(device, ShaderType::Vertex, "Data/Shader/FullScreenQuadVs.cso");
    luminanceExtractionPS = std::make_unique<DX11Shader>(device, ShaderType::Pixel, "Data/Shader/LuminanceExtractionPS.cso");
    uberPostPS = std::make_unique<DX11Shader>(device, ShaderType::Pixel, "Data/Shader/BloomPS.cso");

    constantBuffer = std::make_unique<DX11Buffer>(device, sizeof(CbPostEffect), BufferType::Constant);

    PipelineStateDesc desc{};
    desc.vertexShader = fullscreenQuadVS.get();
    desc.inputLayout = nullptr;
    desc.primitiveTopology = PrimitiveTopology::TriangleStrip;
    desc.depthStencilState = nullptr;
    desc.rasterizerState = nullptr;
    desc.blendState = nullptr;

    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat = TextureFormat::Unknown;

    desc.pixelShader = luminanceExtractionPS.get();
    m_psoLuminance = Graphics::Instance().CreatePipelineState(desc);

    desc.pixelShader = uberPostPS.get();
    m_psoUber = Graphics::Instance().CreatePipelineState(desc);

    const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX11();
    void* scratchBuffer = malloc(scratchBufferSize);
    ffxFsr2GetInterfaceDX11(&m_fsr2Interface, device, scratchBuffer, scratchBufferSize);

    FfxFsr2ContextDescription contextDesc = {};
    contextDesc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_AUTO_EXPOSURE;

    uint32_t screenW = static_cast<uint32_t>(Graphics::Instance().GetScreenWidth());
    uint32_t screenH = static_cast<uint32_t>(Graphics::Instance().GetScreenHeight());
    float renderScale = 0.67f;

    contextDesc.maxRenderSize.width = static_cast<uint32_t>(screenW * renderScale);
    contextDesc.maxRenderSize.height = static_cast<uint32_t>(screenH * renderScale);
    contextDesc.displaySize.width = screenW;
    contextDesc.displaySize.height = screenH;
    contextDesc.callbacks = m_fsr2Interface;
    contextDesc.device = ffxGetDeviceDX11(device);

    FfxErrorCode errorCode = ffxFsr2ContextCreate(&m_fsr2Context, &contextDesc);
    m_fsr2Initialized = (errorCode == FFX_OK);
}

PostEffect::~PostEffect() {
    if (m_fsr2Initialized) {
        ffxFsr2ContextDestroy(&m_fsr2Context);
        free(m_fsr2Interface.scratchBuffer);
    }
}

void PostEffect::LuminanceExtraction(const RenderContext& rc, ITexture* src)
{
    FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
    luminanceFB->SetRenderTarget(rc.commandList, nullptr);

    rc.commandList->SetPipelineState(m_psoLuminance.get());

    rc.commandList->PSSetConstantBuffer(0, constantBuffer.get());
    rc.commandList->PSSetTexture(0, src);

    ISampler* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
    rc.commandList->PSSetSampler(0, sampler);

    rc.commandList->Draw(4, 0);
}

void PostEffect::UberPostProcess(const RenderContext& rc, ITexture* color, ITexture* luminance, ITexture* depth, ITexture* velocity)
{
    FrameBuffer* workFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::PostProcess);
    workFB->SetRenderTarget(rc.commandList, nullptr);

    rc.commandList->SetPipelineState(m_psoUber.get());

    rc.commandList->PSSetConstantBuffer(0, constantBuffer.get());

    ITexture* textures[] = { color, luminance, depth, velocity };
    rc.commandList->PSSetTextures(0, 4, textures);

    ISampler* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
    rc.commandList->PSSetSampler(0, sampler);

    rc.commandList->Draw(4, 0);

    ITexture* nullTextures[4] = { nullptr };
    rc.commandList->PSSetTextures(0, 4, nullTextures);
}

//void PostEffect::Process(const RenderContext& rc, FrameBuffer* src, FrameBuffer* dst)
//{
//    cbPostEffect.time = rc.time;
//    cbPostEffect.luminanceExtractionLowerEdge = rc.bloomData.luminanceLowerEdge;
//    cbPostEffect.luminanceExtractionHigherEdge = rc.bloomData.luminanceHigherEdge;
//    cbPostEffect.bloomIntensity = rc.bloomData.bloomIntensity;
//    cbPostEffect.gaussianSigma = rc.bloomData.gaussianSigma;
//    cbPostEffect.exposure = rc.colorFilterData.exposure;
//    cbPostEffect.monoBlend = rc.colorFilterData.monoBlend;
//    cbPostEffect.hueShift = rc.colorFilterData.hueShift;
//    cbPostEffect.flashAmount = rc.colorFilterData.flashAmount;
//    cbPostEffect.vignetteAmount = rc.colorFilterData.vignetteAmount;
//    cbPostEffect.focusDistance = rc.dofData.focusDistance;
//    cbPostEffect.focusRange = rc.dofData.focusRange;
//    cbPostEffect.bokehRadius = rc.dofData.enable ? rc.dofData.bokehRadius : 0.0f;
//    cbPostEffect.motionBlurIntensity = rc.motionBlurData.intensity;
//    cbPostEffect.motionBlurSamples = rc.motionBlurData.samples;
//
//    rc.commandList->UpdateBuffer(constantBuffer.get(), &cbPostEffect, sizeof(cbPostEffect));
//
//    FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
//    luminanceFB->Clear(rc.commandList, 0, 0, 0, 0);
//    LuminanceExtraction(rc, src->GetColorTexture(0));
//
//    FrameBuffer* gBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer);
//    ITexture* velocityTex = gBuffer ? gBuffer->GetColorTexture(3) : nullptr;
//
//    UberPostProcess(rc, src->GetColorTexture(0), luminanceFB->GetColorTexture(0), rc.sceneDepthTexture, velocityTex);
//
//    rc.commandList->SetRenderTarget(nullptr, nullptr);
//
//    if (m_fsr2Initialized)
//    {
//        ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
//        FfxFsr2DispatchDescription dispatchDesc = {};
//        dispatchDesc.commandList = (FfxCommandList)dc;
//
//        ID3D11Resource* resColor = nullptr, * resDepth = nullptr, * resVelocity = nullptr, * resOutput = nullptr;
//
//        FrameBuffer* workFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::PostProcess);
//        workFB->GetColorMap()->GetResource(&resColor);
//        dispatchDesc.color = ffxGetResourceDX11(&m_fsr2Context, resColor, L"FSR2_InputColor", FFX_RESOURCE_STATE_COMPUTE_READ);
//
//        if (gBuffer && gBuffer->GetDepthMap()) {
//            gBuffer->GetDepthMap()->GetResource(&resDepth);
//            dispatchDesc.depth = ffxGetResourceDX11(&m_fsr2Context, resDepth, L"FSR2_InputDepth", FFX_RESOURCE_STATE_COMPUTE_READ);
//        }
//
//        if (gBuffer && gBuffer->GetColorMap(3)) {
//            gBuffer->GetColorMap(3)->GetResource(&resVelocity);
//            dispatchDesc.motionVectors = ffxGetResourceDX11(&m_fsr2Context, resVelocity, L"FSR2_InputVelocity", FFX_RESOURCE_STATE_COMPUTE_READ);
//        }
//
//        dst->GetRenderTargetView()->GetResource(&resOutput);
//        dispatchDesc.output = ffxGetResourceDX11(&m_fsr2Context, resOutput, L"FSR2_Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
//
//        float rScale = 0.67f;
//        dispatchDesc.motionVectorScale.x = (float)Graphics::Instance().GetScreenWidth() * rScale;
//        dispatchDesc.motionVectorScale.y = (float)Graphics::Instance().GetScreenHeight() * rScale;
//        dispatchDesc.jitterOffset.x = rc.jitterOffset.x;
//        dispatchDesc.jitterOffset.y = rc.jitterOffset.y;
//        dispatchDesc.reset = false;
//        dispatchDesc.enableSharpening = true;
//        dispatchDesc.sharpness = 0.5f;
//
//        static float lastTime = 0.0f;
//        float dT = (rc.time - lastTime) * 1000.0f;
//        dispatchDesc.frameTimeDelta = (dT <= 0.0f) ? 16.6f : dT;
//        lastTime = rc.time;
//
//        dispatchDesc.renderSize.width = static_cast<uint32_t>(Graphics::Instance().GetScreenWidth() * rScale);
//        dispatchDesc.renderSize.height = static_cast<uint32_t>(Graphics::Instance().GetScreenHeight() * rScale);
//        dispatchDesc.preExposure = 1.0f;
//        dispatchDesc.cameraNear = (rc.nearZ > 0.0f) ? rc.nearZ : 0.1f;
//        dispatchDesc.cameraFar = (rc.farZ > 0.0f) ? rc.farZ : 1000.0f;
//        dispatchDesc.cameraFovAngleVertical = (rc.fovY > 0.0f) ? rc.fovY : 1.047f;
//
//        ffxFsr2ContextDispatch(&m_fsr2Context, &dispatchDesc);
//
//        if (resColor) resColor->Release();
//        if (resDepth) resDepth->Release();
//        if (resVelocity) resVelocity->Release();
//        if (resOutput) resOutput->Release();
//    }
//}
//
//

void PostEffect::Process(const RenderContext& rc, ITexture* src, ITexture* dst, ITexture* depth, ITexture* velocity)
{
    cbPostEffect.time = rc.time;
    cbPostEffect.luminanceExtractionLowerEdge = rc.bloomData.luminanceLowerEdge;
    cbPostEffect.luminanceExtractionHigherEdge = rc.bloomData.luminanceHigherEdge;
    cbPostEffect.bloomIntensity = rc.bloomData.bloomIntensity;
    cbPostEffect.gaussianSigma = rc.bloomData.gaussianSigma;
    cbPostEffect.exposure = rc.colorFilterData.exposure;
    cbPostEffect.monoBlend = rc.colorFilterData.monoBlend;
    cbPostEffect.hueShift = rc.colorFilterData.hueShift;
    cbPostEffect.flashAmount = rc.colorFilterData.flashAmount;
    cbPostEffect.vignetteAmount = rc.colorFilterData.vignetteAmount;
    cbPostEffect.focusDistance = rc.dofData.focusDistance;
    cbPostEffect.focusRange = rc.dofData.focusRange;
    cbPostEffect.bokehRadius = rc.dofData.enable ? rc.dofData.bokehRadius : 0.0f;
    cbPostEffect.motionBlurIntensity = rc.motionBlurData.intensity;
    cbPostEffect.motionBlurSamples = rc.motionBlurData.samples;

    rc.commandList->UpdateBuffer(constantBuffer.get(), &cbPostEffect, sizeof(cbPostEffect));

    FrameBuffer* luminanceFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Luminance);
    luminanceFB->Clear(rc.commandList, 0, 0, 0, 0);
    LuminanceExtraction(rc, src);

    UberPostProcess(rc, src, luminanceFB->GetColorTexture(0), depth, velocity);

    rc.commandList->SetRenderTarget(nullptr, nullptr);

    if (m_fsr2Initialized && src && dst)
    {
        ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();
        FfxFsr2DispatchDescription dispatchDesc = {};
        dispatchDesc.commandList = (FfxCommandList)dc;

        // ====================================================
        // ====================================================
        auto getDx11Resource = [](ITexture* tex, ID3D11Resource** outRes) {
            if (!tex) return;
            auto dx11Tex = static_cast<DX11Texture*>(tex);
            if (dx11Tex->GetNativeSRV()) {
                dx11Tex->GetNativeSRV()->GetResource(outRes);
            }
            else if (dx11Tex->GetNativeRTV()) {
                dx11Tex->GetNativeRTV()->GetResource(outRes);
            }
            else if (dx11Tex->GetNativeDSV()) {
                dx11Tex->GetNativeDSV()->GetResource(outRes);
            }
            };

        ID3D11Resource* resColor = nullptr;
        ID3D11Resource* resDepth = nullptr;
        ID3D11Resource* resVelocity = nullptr;
        ID3D11Resource* resOutput = nullptr;

        FrameBuffer* workFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::PostProcess);
        getDx11Resource(workFB->GetColorTexture(0), &resColor);
        if (resColor) {
            dispatchDesc.color = ffxGetResourceDX11(&m_fsr2Context, resColor, L"FSR2_InputColor", FFX_RESOURCE_STATE_COMPUTE_READ);
        }

        // Depth
        getDx11Resource(depth, &resDepth);
        if (resDepth) {
            dispatchDesc.depth = ffxGetResourceDX11(&m_fsr2Context, resDepth, L"FSR2_InputDepth", FFX_RESOURCE_STATE_COMPUTE_READ);
        }

        // Velocity
        getDx11Resource(velocity, &resVelocity);
        if (resVelocity) {
            dispatchDesc.motionVectors = ffxGetResourceDX11(&m_fsr2Context, resVelocity, L"FSR2_InputVelocity", FFX_RESOURCE_STATE_COMPUTE_READ);
        }

        // Output (Display)
        getDx11Resource(dst, &resOutput);
        if (resOutput) {
            dispatchDesc.output = ffxGetResourceDX11(&m_fsr2Context, resOutput, L"FSR2_Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        }

        float rScale = 0.67f;
        dispatchDesc.motionVectorScale.x = (float)Graphics::Instance().GetScreenWidth() * rScale;
        dispatchDesc.motionVectorScale.y = (float)Graphics::Instance().GetScreenHeight() * rScale;
        dispatchDesc.jitterOffset.x = rc.jitterOffset.x;
        dispatchDesc.jitterOffset.y = rc.jitterOffset.y;
        dispatchDesc.reset = false;
        dispatchDesc.enableSharpening = true;
        dispatchDesc.sharpness = 0.5f;

        static float lastTime = 0.0f;
        float dT = (rc.time - lastTime) * 1000.0f;
        dispatchDesc.frameTimeDelta = (dT <= 0.0f) ? 16.6f : dT;
        lastTime = rc.time;

        dispatchDesc.renderSize.width = static_cast<uint32_t>(Graphics::Instance().GetScreenWidth() * rScale);
        dispatchDesc.renderSize.height = static_cast<uint32_t>(Graphics::Instance().GetScreenHeight() * rScale);
        dispatchDesc.preExposure = 1.0f;
        dispatchDesc.cameraNear = (rc.nearZ > 0.0f) ? rc.nearZ : 0.1f;
        dispatchDesc.cameraFar = (rc.farZ > 0.0f) ? rc.farZ : 1000.0f;
        dispatchDesc.cameraFovAngleVertical = (rc.fovY > 0.0f) ? rc.fovY : 1.047f;

        if (resColor && resOutput) {
            ffxFsr2ContextDispatch(&m_fsr2Context, &dispatchDesc);
        }

        if (resColor) resColor->Release();
        if (resDepth) resDepth->Release();
        if (resVelocity) resVelocity->Release();
        if (resOutput) resOutput->Release();
    }
}

void PostEffect::DrawDebugGUI()
{
    ImGui::DragFloat("LuminanceLowerEdge", &cbPostEffect.luminanceExtractionLowerEdge, 0.01f, 0, 1.0f);
    ImGui::DragFloat("LuminanceHigherEdge", &cbPostEffect.luminanceExtractionHigherEdge, 0.01f, 0, 1.0f);
    ImGui::DragFloat("GaussianSigma", &cbPostEffect.gaussianSigma, 0.01f, 0, 10.0f);
    ImGui::DragFloat("BloomIntensity", &cbPostEffect.bloomIntensity, 0.1f, 0, 10.0f);
    ImGui::DragFloat("monoBlend", &cbPostEffect.monoBlend, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("hueShift", &cbPostEffect.hueShift, 0.0f, 0.0f, 1.0f);
    ImGui::DragFloat("flashAmount", &cbPostEffect.flashAmount, 0.0f, 0.0f, 1.0f);
    ImGui::DragFloat("vignetteAmount", &cbPostEffect.vignetteAmount, 0.0f, 0.0f, 1.0f);
}
