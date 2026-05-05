#pragma once

#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class IResourceFactory;

// Game HUD pass. Runs after FinalBlitPass so we draw on the LDR
// DisplayColor (post tone-mapping). The pass owns no PSO of its own;
// it sets up the render target and viewport, then forwards drawing to
// SpriteRenderer + UIManager + DamageTextManager.
//
// See HUD_HPBar_Spec_2026-05-05 v3 section 3.4 for the rationale on
// targeting DisplayColor instead of HDR SceneColor.
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
    ResourceHandle m_hDisplayColor;
};
