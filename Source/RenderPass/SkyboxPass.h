// SkyboxPass のレンダーパス宣言をまとめます。
#pragma once
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <string>

// SceneColor に skybox を描き、既存 depth で背景だけを埋める pass。
class SkyboxPass : public IRenderPass {
public:
    SkyboxPass() = default;
    ~SkyboxPass() override = default;

    std::string GetName() const override { return "SkyboxPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor; // skybox の描画先。
    ResourceHandle m_hDepth;      // scene depth。背景描画の depth test に使う。
};
