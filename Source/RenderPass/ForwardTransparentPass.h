// ForwardTransparentPass のレンダーパス宣言をまとめます。
#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

// Deferred lighting 後の SceneColor に半透明モデルを forward 描画する pass。
class ForwardTransparentPass : public IRenderPass {
public:
    ForwardTransparentPass() = default;
    ~ForwardTransparentPass() override = default;

    std::string GetName() const override { return "ForwardTransparentPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor; // 半透明を合成する HDR color。
    ResourceHandle m_hDepth;      // depth test 用。基本は GBufferDepth。
};
