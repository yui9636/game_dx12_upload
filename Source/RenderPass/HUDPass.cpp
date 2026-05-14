#include "HUDPass.h"

#include "Graphics.h"
#include "Font/FontManager.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RenderContext/RenderContext.h"
#include "RenderGraph/FrameGraphBuilder.h"
#include "RenderGraph/FrameGraphResources.h"
#include "Sprite/SpriteRenderer.h"
#include "UI/DamageTextManager.h"
#include "UI/UI2DSpriteRenderSystem.h"
#include "UI/UI2DTextRenderSystem.h"
#include "UI/UIManager.h"

HUDPass::HUDPass(IResourceFactory*)
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

void HUDPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc)
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

    RenderContext hudRc = rc;
    hudRc.displayWidth = static_cast<uint32_t>(w);
    hudRc.displayHeight = static_cast<uint32_t>(h);
    hudRc.mainViewport = RhiViewport(0.0f, 0.0f, w, h);

    FontManager::Instance().SetRuntimeViewport(w, h);
    SpriteRenderer::Instance().Begin(rc.commandList, { w, h });
    UI2DSpriteRenderSystem::RenderSprites(queue.ui2DSpritePackets, queue.ui2DLayoutNodes, hudRc);
    UI2DTextRenderSystem::RenderText(queue.ui2DTextPackets, queue.ui2DLayoutNodes, hudRc);
    UIManager::Instance().Render(hudRc);
    DamageTextManager::Instance().Render(hudRc);
    SpriteRenderer::Instance().End();
    FontManager::Instance().ClearRuntimeViewport();
}
