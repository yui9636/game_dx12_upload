#include "HUDPass.h"

#include "Graphics.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RenderContext/RenderContext.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "Sprite/SpriteRenderer.h"
#include "UI/DamageTextManager.h"
#include "UI/UIManager.h"

HUDPass::HUDPass(IResourceFactory* /*factory*/)
{
}

void HUDPass::Setup(FrameGraphBuilder& builder, const RenderContext& /*rc*/)
{
    m_hDisplayColor = builder.GetHandle("DisplayColor");
    if (m_hDisplayColor.IsValid()) {
        m_hDisplayColor = builder.Write(m_hDisplayColor);
        builder.RegisterHandle("DisplayColor", m_hDisplayColor);
    }
}

void HUDPass::Execute(FrameGraphResources& resources, const RenderQueue& /*queue*/, RenderContext& rc)
{
    ITexture* displayColor = resources.GetTexture(m_hDisplayColor);
    if (!displayColor || !rc.commandList) {
        return;
    }

    rc.commandList->TransitionBarrier(displayColor, ResourceState::RenderTarget);
    rc.commandList->SetRenderTarget(displayColor, nullptr);
    const float w = static_cast<float>(displayColor->GetWidth());
    const float h = static_cast<float>(displayColor->GetHeight());
    rc.commandList->SetViewport(0.0f, 0.0f, w, h);

    SpriteRenderer::Instance().Begin(rc.commandList, { w, h });
    UIManager::Instance().Render(rc);
    DamageTextManager::Instance().Render(rc);
    SpriteRenderer::Instance().End();
}
