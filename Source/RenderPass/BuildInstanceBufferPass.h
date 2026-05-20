// BuildInstanceBufferPass のレンダーパス宣言をまとめます。
#pragma once
#include "RenderPass/IRenderPass.h"
#include <vector>

// CPU で可視 instance を連続配列へ詰め、GPU へ渡す instance buffer を準備する pass。
class BuildInstanceBufferPass : public IRenderPass {
public:
    std::string GetName() const override { return "BuildInstanceBufferPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // batch ごとの firstInstance / instanceCount を作るための再利用 scratch。
    std::vector<RenderContext::PreparedInstanceBatch> m_batchScratch;
    // InstanceData を連続配置するための再利用 scratch。
    std::vector<InstanceData> m_instanceScratch;
};
