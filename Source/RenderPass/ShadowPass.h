#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class ShadowPass : public IRenderPass {
public:
    ShadowPass() = default;
    ~ShadowPass() override = default;

    std::string GetName() const override { return "ShadowPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hShadowMap;
};
