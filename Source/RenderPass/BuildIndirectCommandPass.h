// BuildIndirectCommandPass のレンダーパス宣言をまとめます。
#pragma once
#include "RenderPass/IRenderPass.h"
#include <vector>

// prepared instance から ExecuteIndirect 用 DrawArgs と command metadata を作る pass。
class BuildIndirectCommandPass : public IRenderPass {
public:
    std::string GetName() const override { return "BuildIndirectCommandPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // batch 単位に並列構築した結果。最後に main thread で連結する。
    struct BatchCommandBuildResult
    {
        std::vector<IndirectDrawCommand> drawCommands;
        std::vector<IndirectDrawCommand> skinnedCommands;
        std::vector<DrawArgs> drawArgs;
        std::vector<RenderContext::GpuDrivenCommandMetadata> metadata;
        std::vector<RenderContext::PreparedIndirectCommand> preparedDrawCommands;
        std::vector<RenderContext::PreparedIndirectCommand> preparedSkinnedCommands;
    };
};
