#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class PostProcessPass : public IRenderPass {
public:
    std::string GetName() const override { return "PostProcessPass"; }

    void Setup(FrameGraphBuilder& builder) override;

    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

    bool HasSideEffects() const override { return true; }

private:
    // ====================================================
    // �� �O���t���ŊǗ�����`�P�b�g�i���͗p�j
    // ====================================================
    ResourceHandle m_hSceneColor;
    ResourceHandle m_hDepth;
    ResourceHandle m_hVelocity;
};