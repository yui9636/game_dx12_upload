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
#include "RenderGraph/FrameGraphResources.h"
#include "Console/Logger.h"

DeferredLightingPass::~DeferredLightingPass() = default;

DeferredLightingPass::DeferredLightingPass(IResourceFactory* factory)
{
    m_vs = factory->CreateShader(ShaderType::Vertex, "Data/Shader/DeferredLightingVS.cso");
    m_ps = factory->CreateShader(ShaderType::Pixel, "Data/Shader/DeferredLightingPS.cso");
    // LUT テクスチャ読み込み（DX11/DX12 共通: ResourceManager が API 自動分岐）
    m_lutGGX = ResourceManager::Instance().GetTexture("Data/Texture/IBL/lut_ggx.dds");

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
}

void DeferredLightingPass::Setup(FrameGraphBuilder& builder)
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
    ITexture* ao = resources.GetTexture(m_hGTAO);
    ITexture* shadow = rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr;
    ITexture* ssgi = resources.GetTexture(m_hSSGI);
    ITexture* fog = resources.GetTexture(m_hFog);
    ITexture* ssr = resources.GetTexture(m_hSSR);
    ITexture* probe = rc.reflectionProbeTexture;
    ITexture* depth = dsReal;
    ITexture* diffuseIBL = rc.environment.diffuseIBLPath.empty()
        ? nullptr
        : ResourceManager::Instance().GetTexture(rc.environment.diffuseIBLPath).get();
    ITexture* specularIBL = rc.environment.specularIBLPath.empty()
        ? nullptr
        : ResourceManager::Instance().GetTexture(rc.environment.specularIBLPath).get();

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        if (dx12Cmd) {
            DX12CommandList::PixelTextureBinding bindings[] = {
                { 0, gbuffer0, DX12CommandList::NullSrvKind::Texture2D },
                { 1, gbuffer1, DX12CommandList::NullSrvKind::Texture2D },
                { 2, gbuffer2, DX12CommandList::NullSrvKind::Texture2D },
                { 3, ao, DX12CommandList::NullSrvKind::Texture2D },
                { 4, shadow, DX12CommandList::NullSrvKind::Texture2DArray },
                { 5, ssgi, DX12CommandList::NullSrvKind::Texture2D },
                { 6, fog, DX12CommandList::NullSrvKind::Texture2D },
                { 7, ssr, DX12CommandList::NullSrvKind::Texture2D },
                { 8, probe, DX12CommandList::NullSrvKind::TextureCube },
                { 9, depth, DX12CommandList::NullSrvKind::Texture2D },
                { 33, diffuseIBL, DX12CommandList::NullSrvKind::TextureCube },
                { 34, specularIBL, DX12CommandList::NullSrvKind::TextureCube },
                { 35, m_lutGGX.get(), DX12CommandList::NullSrvKind::Texture2D },
            };
            dx12Cmd->BindPixelTextureTable(bindings, _countof(bindings));
        }
    } else {
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

    static bool s_loggedOnce = false;
    if (!s_loggedOnce) {
        LOG_INFO("[DeferredLightingPass] LUT=%p reflection=%p shadow=%p diffIBL=%p specIBL=%p",
            m_lutGGX.get(),
            rc.reflectionProbeTexture,
            rc.shadowMap ? rc.shadowMap->GetTexture() : nullptr,
            diffuseIBL,
            specularIBL);
        s_loggedOnce = true;
    }

    rc.commandList->Draw(3, 0);

    if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        rc.commandList->PSSetSampler(2, nullptr);
        rc.commandList->PSSetSampler(3, nullptr);
        rc.commandList->PSSetSampler(1, nullptr);
        ITexture* nulls[11] = { nullptr };
        rc.commandList->PSSetTextures(0, 11, nulls);
        rc.commandList->PSSetTexture(35, nullptr);
    }
}
