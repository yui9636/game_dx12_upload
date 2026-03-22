#pragma once
#include "RenderPass/IRenderPass.h"

class ExtractVisibleInstancesPass : public IRenderPass {
public:
    std::string GetName() const override { return "ExtractVisibleInstancesPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;
};
