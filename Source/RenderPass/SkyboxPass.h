#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <string>

class SkyboxPass : public IRenderPass {
public:
    SkyboxPass() = default;
    ~SkyboxPass() override = default;

    std::string GetName() const override { return "SkyboxPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
};
