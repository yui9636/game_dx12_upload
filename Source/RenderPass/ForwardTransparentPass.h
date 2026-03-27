#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class ForwardTransparentPass : public IRenderPass {
public:
    ForwardTransparentPass() = default;
    ~ForwardTransparentPass() override = default;

    std::string GetName() const override { return "ForwardTransparentPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
};
