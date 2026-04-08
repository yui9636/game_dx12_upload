#include "EffectMeshPass.h"

#include "Graphics.h"
#include "Model/ModelRenderer.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"

void EffectMeshPass::Setup(FrameGraphBuilder& builder, const RenderContext&)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");

    if (m_hSceneColor.IsValid()) {
        builder.Read(m_hSceneColor);
    }
    if (m_hDepth.IsValid()) {
        builder.Read(m_hDepth);
    }
}

void EffectMeshPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    if (queue.effectMeshPackets.empty()) {
        return;
    }

    ITexture* rtScene = resources.GetTexture(m_hSceneColor);
    ITexture* dsReal = resources.GetTexture(m_hDepth);
    if (!rtScene || !dsReal) {
        return;
    }

    auto renderer = rc.modelRendererOverride ? rc.modelRendererOverride : Graphics::Instance().GetModelRenderer();
    if (!renderer) {
        return;
    }

    rc.commandList->TransitionBarrier(rtScene, ResourceState::RenderTarget);
    rc.commandList->TransitionBarrier(dsReal, ResourceState::DepthWrite);
    rc.commandList->SetRenderTarget(rtScene, dsReal);
    rc.mainRenderTarget = rtScene;
    rc.mainDepthStencil = dsReal;
    rc.mainViewport = RhiViewport(0.0f, 0.0f, static_cast<float>(rtScene->GetWidth()), static_cast<float>(rtScene->GetHeight()));
    rc.commandList->SetViewport(rc.mainViewport);

    renderer->SetIBL(rc.environment.diffuseIBLPath, rc.environment.specularIBLPath);

    for (const auto& packet : queue.effectMeshPackets) {
        if (!packet.modelResource) {
            continue;
        }

        renderer->Draw(
            static_cast<ShaderId>(packet.shaderId),
            packet.modelResource,
            packet.worldMatrix,
            packet.prevWorldMatrix,
            packet.baseColor,
            packet.metallic,
            packet.roughness,
            packet.emissive,
            packet.materialAsset.get(),
            packet.blendState,
            packet.depthState,
            packet.rasterizerState);
    }

    renderer->RenderTransparent(rc);
    rc.commandList->SetRenderTarget(nullptr, nullptr);
}
