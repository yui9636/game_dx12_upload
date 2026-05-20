#pragma once

#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class IResourceFactory;

// Game HUD pass は FinalBlitPass の後に実行し、LDR の
// DisplayColor へ描画する。pass 自身は PSO を持たず、
// render target と viewport を設定してから描画を
// SpriteRenderer、UIManager、DamageTextManager へ委譲する。
// DisplayColor を対象にする理由は HUD_HPBar_Spec_2026-05-05 v3 の 3.4 節にある。
// HDR SceneColor ではなく tone mapping 後の DisplayColor を使う。
class HUDPass : public IRenderPass
{
public:
    explicit HUDPass(IResourceFactory* factory);
    ~HUDPass() override = default;

    std::string GetName() const override { return "HUDPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // tone mapping 後の DisplayColor。HUD はここに直接描く。
    ResourceHandle m_hDisplayColor;
};
