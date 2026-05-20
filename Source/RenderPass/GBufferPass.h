#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

// 不透明モデルを複数 render target へ描き、DeferredLighting の入力 GBuffer を作る pass。
class GBufferPass : public IRenderPass {
public:
    std::string GetName() const override { return "GBufferPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;

    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hGBuffer0; // Albedo と Metallic。
    ResourceHandle m_hGBuffer1; // 法線と Roughness。
    ResourceHandle m_hGBuffer2; // WorldPos 用。 + Depth(A)
    ResourceHandle m_hGBuffer3; // Velocity を RG へ格納する。
    ResourceHandle m_hDepth;    // DepthStencil 用。
};
