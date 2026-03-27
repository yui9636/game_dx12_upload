#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class PostProcessPass : public IRenderPass {
public:
    std::string GetName() const override { return "PostProcessPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;

    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

    bool HasSideEffects() const override { return true; }

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
    ResourceHandle m_hVelocity;
    ResourceHandle m_hDisplayColor;
};
