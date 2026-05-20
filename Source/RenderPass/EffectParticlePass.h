// EffectParticlePass のレンダーパス宣言をまとめます。
#pragma once

#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

// GPU particle の simulation / sort / indirect draw をまとめて行う DX12 専用 pass。
class EffectParticlePass : public IRenderPass
{
public:
    EffectParticlePass() = default;
    ~EffectParticlePass() override = default;

    std::string GetName() const override { return "EffectParticlePass"; }
    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor; // particle を合成する HDR scene color。
    ResourceHandle m_hDepth;      // depth test / depth binning 用。
};
