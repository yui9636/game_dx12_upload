// PostProcessPass のレンダーパス実装をまとめます。
#include "PostProcessPass.h"

#include "Render/Graphics.h"
#include "PostEffect/PostEffect.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RenderGraph/FrameGraphResources.h"

void PostProcessPass::Setup(FrameGraphBuilder& builder, const RenderContext& rc)
{
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");
    m_hVelocity = builder.GetHandle("GBuffer3");
    m_hDisplayColor = builder.GetHandle("DisplayColor");

    if (m_hSceneColor.IsValid()) builder.Read(m_hSceneColor);
    if (m_hDepth.IsValid()) builder.Read(m_hDepth);
    if (m_hVelocity.IsValid()) builder.Read(m_hVelocity);
    if (m_hDisplayColor.IsValid()) {
        m_hDisplayColor = builder.Write(m_hDisplayColor);
        builder.RegisterHandle("DisplayColor", m_hDisplayColor);
    }
}

void PostProcessPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
{
    Graphics& graphics = Graphics::Instance();
    auto* postEffect = graphics.GetPostEffect();
    if (!rc.enablePostProcess || !postEffect || !rc.commandList) {
        return;
    }

    ITexture* srcTex = resources.GetTexture(m_hSceneColor);
    ITexture* depthTex = resources.GetTexture(m_hDepth);
    ITexture* velTex = resources.GetTexture(m_hVelocity);
    ITexture* dstTex = resources.GetTexture(m_hDisplayColor);

    if (!dstTex) {
        FrameBuffer* displayFB = graphics.GetFrameBuffer(FrameBufferId::Display);
        dstTex = displayFB ? displayFB->GetColorTexture(0) : nullptr;
    }
    if (!srcTex || !dstTex) {
        return;
    }

    if (graphics.GetAPI() == GraphicsAPI::DX12) {
        rc.commandList->TransitionBarrier(srcTex, ResourceState::ShaderResource);
        if (depthTex) rc.commandList->TransitionBarrier(depthTex, ResourceState::ShaderResource);
        if (velTex) rc.commandList->TransitionBarrier(velTex, ResourceState::ShaderResource);
    }

    postEffect->Process(rc, srcTex, dstTex, depthTex, velTex);
}
