#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class DrawObjectsPass : public IRenderPass {
public:
    DrawObjectsPass() = default;
    ~DrawObjectsPass() override = default;

    std::string GetName() const override { return "DrawObjectsPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
};