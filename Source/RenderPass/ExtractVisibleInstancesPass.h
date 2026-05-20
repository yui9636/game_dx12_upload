// ExtractVisibleInstancesPass のレンダーパス宣言をまとめます。
#pragma once
#include "RenderPass/IRenderPass.h"
#include <vector>

// RenderQueue の opaque instance を view frustum で CPU culling し、可視 batch を作る pass。
class ExtractVisibleInstancesPass : public IRenderPass {
public:
    std::string GetName() const override { return "ExtractVisibleInstancesPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // 並列 culling の一時出力。batch index と同じ並びで結果を保持する。
    std::vector<InstanceBatch> m_candidateBatches;
    // candidateBatches のうち有効なものだけを後で詰めるためのフラグ。
    std::vector<uint8_t> m_nonEmptyFlags;
};
