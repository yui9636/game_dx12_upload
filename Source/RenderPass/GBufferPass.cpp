//#include "GBufferPass.h"
//#include "Graphics.h"
//#include "Model/ModelRenderer.h"
#include "Model/Model.h"
//#include "RHI/ICommandList.h"
//
//void GBufferPass::Setup(FrameGraphBuilder& builder)
//{
//}
//
//
//void GBufferPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
//    Graphics& g = Graphics::Instance();
//
//    // 1. GBufferをセット・クリア
//    FrameBuffer* gbuffer = g.GetFrameBuffer(FrameBufferId::GBuffer);
//    gbuffer->Clear(rc.commandList, 0.0f, 0.0f, 0.0f, 0.0f);
//    gbuffer->SetRenderTargets(rc.commandList);
//
//    auto renderer = g.GetModelRenderer();
//    if (!renderer) return;
//    for (const auto& packet : queue.opaquePackets) {
//        if (!packet.model) continue;
//
//        std::shared_ptr<Model> sharedModel(packet.model, [](Model*) {});
//
//        renderer->Draw(
//            ShaderId::GBufferPBR, sharedModel, packet.worldMatrix, packet.prevWorldMatrix,
//            packet.baseColor, packet.metallic, packet.roughness, packet.emissive,
//            packet.blendState, packet.depthState, packet.rasterizerState
//        );
//    }
//
//    // 3. 不透明オブジェクトだけを一気に描画！
//    // (内部で rc.commandList が活用されます)
//    renderer->RenderOpaque(rc);
//}
//
#include "GBufferPass.h"
#include "Graphics.h"
#include "Model/ModelRenderer.h"
#include "Model/Model.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/DX12/DX12Texture.h"
#include "RenderGraph/FrameGraphResources.h"
#include "Console/Logger.h"

void GBufferPass::Setup(FrameGraphBuilder& builder)
{
 
    float renderScale = Graphics::Instance().GetRenderScale();
    uint32_t w = (uint32_t)(Graphics::Instance().GetScreenWidth() * renderScale);
    uint32_t h = (uint32_t)(Graphics::Instance().GetScreenHeight() * renderScale);


    // 1. シェーダーの要求するフォーマットで内部テクスチャを作成
    TextureDesc desc0{}; // Slot 0: Albedo / Metallic
    desc0.width = w; desc0.height = h;
    desc0.format = TextureFormat::R16G16B16A16_FLOAT;
    desc0.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    m_hGBuffer0 = builder.CreateTexture("GBuffer0", desc0);

    TextureDesc desc1{}; // Slot 1: Normal / Roughness
    desc1.width = w; desc1.height = h;
    desc1.format = TextureFormat::R16G16B16A16_FLOAT;
    desc1.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    m_hGBuffer1 = builder.CreateTexture("GBuffer1", desc1);

    TextureDesc desc2{}; // Slot 2: WorldPos / Depth
    desc2.width = w; desc2.height = h;
    desc2.format = TextureFormat::R32G32B32A32_FLOAT;
    desc2.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    m_hGBuffer2 = builder.CreateTexture("GBuffer2", desc2);

    TextureDesc desc3{}; // Slot 3: Velocity
    desc3.width = w; desc3.height = h;
    desc3.format = TextureFormat::R32G32_FLOAT;
    desc3.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    m_hGBuffer3 = builder.CreateTexture("GBuffer3", desc3);

    TextureDesc depthDesc{}; // Depth Stencil
    depthDesc.width = w; depthDesc.height = h;
    depthDesc.format = TextureFormat::D32_FLOAT;
    depthDesc.bindFlags = TextureBindFlags::DepthStencil | TextureBindFlags::ShaderResource;
    m_hDepth = builder.CreateTexture("GBufferDepth", depthDesc);

    // 2. 書き込み宣言と掲示板（Blackboard）への登録
    // これをしないと DeferredLightingPass が GetHandle できません。
    m_hGBuffer0 = builder.Write(m_hGBuffer0);
    m_hGBuffer1 = builder.Write(m_hGBuffer1);
    m_hGBuffer2 = builder.Write(m_hGBuffer2);
    m_hGBuffer3 = builder.Write(m_hGBuffer3);
    m_hDepth = builder.Write(m_hDepth);

    builder.RegisterHandle("GBuffer0", m_hGBuffer0);
    builder.RegisterHandle("GBuffer1", m_hGBuffer1);
    builder.RegisterHandle("GBuffer2", m_hGBuffer2);
    builder.RegisterHandle("GBuffer3", m_hGBuffer3);
    builder.RegisterHandle("GBufferDepth", m_hDepth);
}

void GBufferPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    // グラフから実体を取得
    ITexture* rtvs[] = {
        resources.GetTexture(m_hGBuffer0),
        resources.GetTexture(m_hGBuffer1),
        resources.GetTexture(m_hGBuffer2),
        resources.GetTexture(m_hGBuffer3)
    };
    ITexture* dsv = resources.GetTexture(m_hDepth);

    static bool s_missingTargetsLogged = false;
    if (!rtvs[0] || !dsv) {
        if (!s_missingTargetsLogged) {
            LOG_ERROR("[GBufferPass] Missing render targets rt0=%p depth=%p", rtvs[0], dsv);
            s_missingTargetsLogged = true;
        }
        return;
    }

    static bool s_loggedDepthResource = false;
    if (!s_loggedDepthResource && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        if (auto* dx12Depth = dynamic_cast<DX12Texture*>(dsv)) {
            LOG_INFO("[GBufferPass] depthResource=%p", dx12Depth->GetNativeResource());
            s_loggedDepthResource = true;
        }
    }

    // クリアとターゲット設定
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 4; ++i) rc.commandList->ClearColor(rtvs[i], clearColor);
    rc.commandList->ClearDepthStencil(dsv, 1.0f, 0);

    rc.commandList->SetRenderTargets(4, rtvs, dsv);

    // RenderContext の同期
    rc.mainRenderTarget = rtvs[0];
    rc.mainDepthStencil = dsv;
    rc.sceneColorTexture = rtvs[0];
    rc.sceneDepthTexture = dsv;
    rc.debugGBuffer0 = rtvs[0];
    rc.debugGBuffer1 = rtvs[1];
    rc.debugGBuffer2 = rtvs[2];
    rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)rtvs[0]->GetWidth(), (float)rtvs[0]->GetHeight());
    rc.commandList->SetViewport(rc.mainViewport);

    // モデル描画
    auto renderer = Graphics::Instance().GetModelRenderer();
    if (!renderer) {
        LOG_ERROR("[GBufferPass] ModelRenderer is null");
        return;
    }

    static size_t s_lastOpaqueCount = static_cast<size_t>(-1);
    if (s_lastOpaqueCount != queue.opaquePackets.size()) {
        LOG_INFO("[GBufferPass] opaque=%zu size=%ux%u", queue.opaquePackets.size(), rtvs[0]->GetWidth(), rtvs[0]->GetHeight());
        s_lastOpaqueCount = queue.opaquePackets.size();
    }

    for (const auto& packet : queue.opaquePackets) {
        if (!packet.modelResource) continue;
        renderer->Draw(
            ShaderId::GBufferPBR, packet.modelResource, packet.worldMatrix, packet.prevWorldMatrix,
            packet.baseColor, packet.metallic, packet.roughness, packet.emissive,
            packet.blendState, packet.depthState, packet.rasterizerState
        );
    }
    renderer->RenderOpaque(rc);


}
