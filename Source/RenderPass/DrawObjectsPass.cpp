#include "DrawObjectsPass.h"
#include "Graphics.h"
#include "Model/ModelRenderer.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"

void DrawObjectsPass::Setup(FrameGraphBuilder& builder)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth      = builder.GetHandle("GBufferDepth");

    if (m_hSceneColor.IsValid()) {
        builder.Read(m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void DrawObjectsPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsDepth = resources.GetTexture(m_hDepth);

    if (!rtScene || !dsDepth) return;

    // レンダーターゲットをバインド
    rc.commandList->TransitionBarrier(rtScene, ResourceState::RenderTarget);
    rc.commandList->TransitionBarrier(dsDepth, ResourceState::DepthWrite);
    rc.commandList->SetRenderTarget(rtScene, dsDepth);
    rc.mainRenderTarget = rtScene;
    rc.mainDepthStencil = dsDepth;
    rc.mainViewport     = RhiViewport(0.0f, 0.0f, (float)rtScene->GetWidth(), (float)rtScene->GetHeight());
    rc.commandList->SetViewport(rc.mainViewport);

    Graphics& g = Graphics::Instance();
    auto renderer = g.GetModelRenderer();
    if (!renderer) return;

    renderer->SetIBL(rc.environment.diffuseIBLPath, rc.environment.specularIBLPath);

    // 不透明オブジェクト
    for (const auto& packet : queue.opaquePackets) {
        if (!packet.model) continue;
        std::shared_ptr<Model> sharedModel(packet.model, [](Model*) {});
        renderer->Draw(
            static_cast<ShaderId>(packet.shaderId), sharedModel,
            packet.worldMatrix, packet.prevWorldMatrix,
            packet.baseColor, packet.metallic, packet.roughness, packet.emissive,
            packet.blendState, packet.depthState, packet.rasterizerState
        );
    }

    // 半透明オブジェクト
    for (const auto& packet : queue.transparentPackets) {
        if (!packet.model) continue;
        std::shared_ptr<Model> sharedModel(packet.model, [](Model*) {});
        renderer->Draw(
            static_cast<ShaderId>(packet.shaderId), sharedModel,
            packet.worldMatrix, packet.prevWorldMatrix,
            packet.baseColor, packet.metallic, packet.roughness, packet.emissive,
            packet.blendState, packet.depthState, packet.rasterizerState
        );
    }

    renderer->Render(rc, queue);

    rc.commandList->SetRenderTarget(nullptr, nullptr);
}
