#pragma once
#include "RenderPass/IRenderPass.h"

class BuildInstanceBufferPass : public IRenderPass {
public:
    std::string GetName() const override { return "BuildInstanceBufferPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;
};
