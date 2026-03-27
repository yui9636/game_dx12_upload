#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class GBufferPass : public IRenderPass {
public:
    std::string GetName() const override { return "GBufferPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;

    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // ====================================================
    // ====================================================
    ResourceHandle m_hGBuffer0; // Albedo + Metallic
    ResourceHandle m_hGBuffer1; // Normal + Roughness
    ResourceHandle m_hGBuffer2; // WorldPos + Depth(A)
    ResourceHandle m_hGBuffer3; // Velocity (RG)
    ResourceHandle m_hDepth;    // DepthStencil
};
