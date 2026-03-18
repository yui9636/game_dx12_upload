#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h" // �n���h���p

class ForwardTransparentPass : public IRenderPass {
public:
    ForwardTransparentPass() = default;
    ~ForwardTransparentPass() override = default;

    // �� �C���F���������O��Ԃ��悤�ɕύX
    std::string GetName() const override { return "ForwardTransparentPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
};
