#include "PostProcessPass.h"
#include "../Graphics.h"
#include "../PostEffect.h"
#include "RenderGraph/FrameGraphResources.h"

void PostProcessPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");
    m_hVelocity = builder.GetHandle("GBuffer3");
    m_hDisplayColor = builder.GetHandle("DisplayColor");

    if (m_hSceneColor.IsValid()) builder.Read(m_hSceneColor);
    if (m_hDepth.IsValid())      builder.Read(m_hDepth);
    if (m_hVelocity.IsValid())   builder.Read(m_hVelocity);
    if (m_hDisplayColor.IsValid()) {
        m_hDisplayColor = builder.Write(m_hDisplayColor);
        builder.RegisterHandle("DisplayColor", m_hDisplayColor);
    }
}

void PostProcessPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    Graphics& g = Graphics::Instance();
    auto postEffect = g.GetPostEffect();
    if (!postEffect) return;

    ITexture* srcTex = resources.GetTexture(m_hSceneColor);
    ITexture* depthTex = resources.GetTexture(m_hDepth);
    ITexture* velTex = resources.GetTexture(m_hVelocity);
    ITexture* dstTex = resources.GetTexture(m_hDisplayColor);

    if (!dstTex) {
        FrameBuffer* displayFB = g.GetFrameBuffer(FrameBufferId::Display);
        dstTex = displayFB ? displayFB->GetColorTexture(0) : nullptr;
    }

    if (!srcTex || !dstTex) return;

    postEffect->Process(rc, srcTex, dstTex, depthTex, velTex);
}
