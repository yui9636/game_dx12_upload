// DrawObjectsPass のレンダーパス宣言をまとめます。
#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

// SceneColor / Depth に forward 経路で通常モデルを描画する pass。
class DrawObjectsPass : public IRenderPass {
public:
    DrawObjectsPass() = default;
    ~DrawObjectsPass() override = default;

    std::string GetName() const override { return "DrawObjectsPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor; // 描画先 HDR color。
    ResourceHandle m_hDepth;      // depth test/write に使う GBufferDepth。
};
