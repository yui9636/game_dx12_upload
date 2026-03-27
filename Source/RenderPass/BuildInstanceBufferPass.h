#pragma once
#include "RenderPass/IRenderPass.h"
#include <vector>

class BuildInstanceBufferPass : public IRenderPass {
public:
    std::string GetName() const override { return "BuildInstanceBufferPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::vector<RenderContext::PreparedInstanceBatch> m_batchScratch;
    std::vector<InstanceData> m_instanceScratch;
};
