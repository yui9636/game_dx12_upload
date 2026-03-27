#pragma once
#include "RenderPass/IRenderPass.h"
#include <vector>

class BuildIndirectCommandPass : public IRenderPass {
public:
    std::string GetName() const override { return "BuildIndirectCommandPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    struct BatchCommandBuildResult
    {
        std::vector<IndirectDrawCommand> drawCommands;
        std::vector<IndirectDrawCommand> skinnedCommands;
        std::vector<DrawArgs> drawArgs;
        std::vector<RenderContext::GpuDrivenCommandMetadata> metadata;
        std::vector<RenderContext::PreparedIndirectCommand> preparedDrawCommands;
        std::vector<RenderContext::PreparedIndirectCommand> preparedSkinnedCommands;
    };
    std::vector<BatchCommandBuildResult> m_batchResults;
    std::vector<DrawArgs> m_drawArgsScratch;
    std::vector<RenderContext::GpuDrivenCommandMetadata> m_metadataScratch;
};
