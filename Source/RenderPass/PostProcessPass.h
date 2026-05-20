// PostProcessPass のレンダーパス宣言をまとめます。
#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

// SceneColor にポストエフェクトをかけ、DisplayColor へ出力する pass。
class PostProcessPass : public IRenderPass {
public:
    std::string GetName() const override { return "PostProcessPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;

    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

    bool HasSideEffects() const override { return true; }

private:
    ResourceHandle m_hSceneColor;   // 入力 HDR scene color。
    ResourceHandle m_hDepth;        // depth-based effect 用。
    ResourceHandle m_hVelocity;     // TAA / motion blur 系 effect 用 velocity。
    ResourceHandle m_hDisplayColor; // 出力 display color。
};
