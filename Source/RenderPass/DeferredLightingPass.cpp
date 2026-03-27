#include "DeferredLightingPass.h"
#include "Graphics.h"
#include "System/ResourceManager.h"
#include "RHI/ITexture.h"
#include "RHI/IShader.h"
#include "RHI/IResourceFactory.h"
#include "RHI/ICommandList.h"
#include "RHI/PipelineStateDesc.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"
#include "RHI/DX12/DX12RootSignature.h"
#include "RenderGraph/FrameGraphResources.h"
#include "Console/Logger.h"

namespace {
    D3D12_CPU_DESCRIPTOR_HANDLE OffsetHandle(D3D12_CPU_DESCRIPTOR_HANDLE base, UINT stride, UINT index) {
        base.ptr += static_cast<SIZE_T>(stride) * index;
        return base;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDeferredNullHandle(
        uint32_t slot,
        D3D12_CPU_DESCRIPTOR_HANDLE null2D,
        D3D12_CPU_DESCRIPTOR_HANDLE null2DArray,
        D3D12_CPU_DESCRIPTOR_HANDLE nullCube)
    {
        if (slot == 4) return null2DArray;
        if (slot == 8 || slot == 33 || slot == 34) return nullCube;
        return null2D;
    }
}

DeferredLightingPass::~DeferredLightingPass() = default;

DeferredLightingPass::DeferredLightingPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DeferredLightingVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/DeferredLightingPS.cso");
    // LUT テクスチャ読み込み（DX11/DX12 共通: ResourceManager が API 自動分岐）
    m_lutGGX = ResourceManager::Instance().GetTexture("Data/Texture/IBL/lut_ggx.dds");
    m_whiteFallback = ResourceManager::Instance().GetTexture("Data/Texture/UI/White.png");

    auto* rs = Graphics::Instance().GetRenderState();
    PipelineStateDesc desc{};
    desc.vertexShader = m_vs.get();
    desc.pixelShader = m_ps.get();
    desc.inputLayout = nullptr;
    desc.depthStencilState = rs->GetDepthStencilState(DepthState::NoTestNoWrite);
    desc.rasterizerState = rs->GetRasterizerState(RasterizerState::SolidCullNone);
    desc.blendState = rs->GetBlendState(BlendState::Opaque);
    desc.primitiveTopology = PrimitiveTopology::TriangleList;
    desc.numRenderTargets = 1;
    desc.rtvFormats[0] = TextureFormat::R16G16B16A16_FLOAT;
    desc.dsvFormat = TextureFormat::Unknown;

    m_pso = factory->CreatePipelineState(desc);

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Device = Graphics::Instance().GetDX12Device();
        auto* d3dDevice = dx12Device ? dx12Device->GetDevice() : nullptr;
        if (d3dDevice) {
            m_dx12SrvDescriptorSize = dx12Device->GetCBVSRVUAVDescriptorSize();

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = 64;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            HRESULT hr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dx12SrvHeap));
            if (SUCCEEDED(hr) && m_dx12SrvHeap) {
                auto cpuBase = m_dx12SrvHeap->GetCPUDescriptorHandleForHeapStart();
                m_dx12SrvGpuBase = m_dx12SrvHeap->GetGPUDescriptorHandleForHeapStart();

                m_dx12NullSrv2D = dx12Device->AllocateSRVDescriptor();
                m_dx12NullSrv2DArray = dx12Device->AllocateSRVDescriptor();
                m_dx12NullSrvCube = dx12Device->AllocateSRVDescriptor();

                D3D12_SHADER_RESOURCE_VIEW_DESC null2DDesc = {};
                null2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                null2DDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                null2DDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                null2DDesc.Texture2D.MipLevels = 1;
                d3dDevice->CreateShaderResourceView(nullptr, &null2DDesc, m_dx12NullSrv2D);

                D3D12_SHADER_RESOURCE_VIEW_DESC null2DArrayDesc = {};
                null2DArrayDesc.Format = DXGI_FORMAT_R32_FLOAT;
                null2DArrayDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                null2DArrayDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                null2DArrayDesc.Texture2DArray.MipLevels = 1;
                null2DArrayDesc.Texture2DArray.ArraySize = 1;
                d3dDevice->CreateShaderResourceView(nullptr, &null2DArrayDesc, m_dx12NullSrv2DArray);

                D3D12_SHADER_RESOURCE_VIEW_DESC nullCubeDesc = {};
                nullCubeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                nullCubeDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                nullCubeDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                nullCubeDesc.TextureCube.MipLevels = 1;
                d3dDevice->CreateShaderResourceView(nullptr, &nullCubeDesc, m_dx12NullSrvCube);

                for (UINT slot = 0; slot < 64; ++slot) {
                    auto dst = OffsetHandle(cpuBase, m_dx12SrvDescriptorSize, slot);
                    auto src = GetDeferredNullHandle(slot, m_dx12NullSrv2D, m_dx12NullSrv2DArray, m_dx12NullSrvCube);
                    d3dDevice->CopyDescriptorsSimple(1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
            }
        }
    }
}

void DeferredLightingPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hGBuffer0 = builder.GetHandle("GBuffer0");
    m_hGBuffer1 = builder.GetHandle("GBuffer1");
    m_hGBuffer2 = builder.GetHandle("GBuffer2");
    m_hGBuffer3 = builder.GetHandle("GBuffer3");
    m_hDepth = builder.GetHandle("GBufferDepth");

    m_hGTAO = builder.GetHandle("GTAO");
    m_hSSGI = builder.GetHandle("SSGIBlur");
    m_hFog = builder.GetHandle("VolumetricFogBlur");
    m_hSSR = builder.GetHandle("SSRBlur");
    m_hSceneColor = builder.GetHandle("SceneColor");

    if (m_hGBuffer0.IsValid()) builder.Read(m_hGBuffer0);
    if (m_hGBuffer1.IsValid()) builder.Read(m_hGBuffer1);
    if (m_hGBuffer2.IsValid()) builder.Read(m_hGBuffer2);
    if (m_hGBuffer3.IsValid()) builder.Read(m_hGBuffer3);
    if (m_hDepth.IsValid())    builder.Read(m_hDepth);

    if (m_hGTAO.IsValid()) builder.Read(m_hGTAO);
    if (m_hSSGI.IsValid()) builder.Read(m_hSSGI);
    if (m_hFog.IsValid())  builder.Read(m_hFog);
    if (m_hSSR.IsValid())  builder.Read(m_hSSR);

    if (m_hSceneColor.IsValid()) {
        m_hSceneColor = builder.Write(m_hSceneColor);
        builder.RegisterHandle("SceneColor", m_hSceneColor);
    }
}

void DeferredLightingPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal = resources.GetTexture(m_hDepth);

    static bool s_missingTargetsLogged = false;
    if (!rtScene || !dsReal) {
        if (!s_missingTargetsLogged) {
            LOG_ERROR("[DeferredLightingPass] Missing scene/depth rt=%p depth=%p", rtScene, dsReal);
            s_missingTargetsLogged = true;
        }
        return;
    }

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    rc.commandList->ClearColor(rtScene, clearColor);
    rc.commandList->SetRenderTarget(rtScene, nullptr);

    // ★ Context同期：これをしないと後の SkyboxPass 等が「空の深度」を使ってキャラを消します
    rc.mainRenderTarget = rtScene;
    rc.mainDepthStencil = dsReal;
    rc.sceneColorTexture = rtScene;
    rc.sceneDepthTexture = dsReal;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)rtScene->GetWidth(), (float)rtScene->GetHeight());
    rc.commandList->SetViewport(rc.mainViewport);

    rc.commandList->SetPipelineState(m_pso.get());

    auto* pointSampler = rc.renderState->GetSamplerState(SamplerState::PointClamp);
    auto* linearSampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
    rc.commandList->PSSetSampler(2, pointSampler);
    rc.commandList->PSSetSampler(3, linearSampler);
    if (rc.shadowMap) rc.commandList->PSSetSampler(1, rc.shadowMap->GetSamplerState());

    ITexture* gbuffer0 = resources.GetTexture(m_hGBuffer0);
    ITexture* gbuffer1 = resources.GetTexture(m_hGBuffer1);
    ITexture* gbuffer2 = resources.GetTexture(m_hGBuffer2);
    ITexture* ao = rc.enableGTAO ? resources.GetTexture(m_hGTAO) : nullptr;
    ITexture* shadow = rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr;
    ITexture* ssgi = rc.enableSSGI ? resources.GetTexture(m_hSSGI) : nullptr;
    ITexture* fog = rc.enableVolumetricFog ? resources.GetTexture(m_hFog) : nullptr;
    ITexture* ssr = rc.enableSSR ? resources.GetTexture(m_hSSR) : nullptr;
    ITexture* probe = rc.reflectionProbeTexture;
    ITexture* depth = dsReal;
    ITexture* diffuseIBL = rc.environment.diffuseIBLPath.empty()
        ? nullptr
        : ResourceManager::Instance().GetTexture(rc.environment.diffuseIBLPath).get();
    ITexture* specularIBL = rc.environment.specularIBLPath.empty()
        ? nullptr
        : ResourceManager::Instance().GetTexture(rc.environment.specularIBLPath).get();

    static uint32_t s_shadowInputLogCount = 0;
    if (s_shadowInputLogCount < 8) {
        auto* dx12Shadow = dynamic_cast<DX12Texture*>(shadow);
        LOG_INFO(
            "[DeferredShadowInput] frame=%u shadow=%p state=%d hasSRV=%d ao=%p ssgi=%p fog=%p ssr=%p",
            s_shadowInputLogCount,
            shadow,
            shadow ? static_cast<int>(shadow->GetCurrentState()) : -1,
            dx12Shadow && dx12Shadow->HasSRV() ? 1 : 0,
            ao,
            ssgi,
            fog,
            ssr);
        ++s_shadowInputLogCount;
    }

    if (!ao && m_whiteFallback) {
        ao = m_whiteFallback.get();
    }

    // Use standard PSSetTextures path for all APIs.
    // Avoids SetDescriptorHeaps thrashing which invalidates root descriptor tables.
    {
        ITexture* gbuffers[] = { gbuffer0, gbuffer1, gbuffer2, ao };
        rc.commandList->PSSetTextures(0, 4, gbuffers);
        if (shadow) rc.commandList->PSSetTexture(4, shadow);
        rc.commandList->PSSetTexture(5, ssgi);
        rc.commandList->PSSetTexture(6, fog);
        rc.commandList->PSSetTexture(7, ssr);
        if (probe) rc.commandList->PSSetTexture(8, probe);
        rc.commandList->PSSetTexture(9, depth);
        rc.commandList->PSSetTexture(33, diffuseIBL);
        rc.commandList->PSSetTexture(34, specularIBL);
        rc.commandList->PSSetTexture(35, m_lutGGX.get());
    }

    rc.commandList->Draw(3, 0);

    // PSSetTextures path uses the frame heap — no heap restore needed.

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetSampler(2, nullptr);
        rc.commandList->PSSetSampler(3, nullptr);
        rc.commandList->PSSetSampler(1, nullptr);
        ITexture* nulls[11] = { nullptr };
        rc.commandList->PSSetTextures(0, 11, nulls);
        rc.commandList->PSSetTexture(35, nullptr);
    }
}
