// ShadowPass のレンダーパス宣言をまとめます。
#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

// Cascaded shadow map を更新し、後段の lighting から SRV として読める状態へ戻す pass。
class ShadowPass : public IRenderPass {
public:
    ShadowPass() = default;
    ~ShadowPass() override = default;

    std::string GetName() const override { return "ShadowPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // Graphics 側で ImportTexture された ShadowMap texture の handle。
    ResourceHandle m_hShadowMap;
};
