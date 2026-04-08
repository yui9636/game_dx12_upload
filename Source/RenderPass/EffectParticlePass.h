#pragma once

#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class EffectParticlePass : public IRenderPass
{
public:
    EffectParticlePass() = default;
    ~EffectParticlePass() override = default;

    std::string GetName() const override { return "EffectParticlePass"; }
    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
};
