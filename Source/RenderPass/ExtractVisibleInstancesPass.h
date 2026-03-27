#pragma once
#include "RenderPass/IRenderPass.h"
#include <vector>

class ExtractVisibleInstancesPass : public IRenderPass {
public:
    std::string GetName() const override { return "ExtractVisibleInstancesPass"; }
    bool HasSideEffects() const override { return true; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    std::vector<InstanceBatch> m_candidateBatches;
    std::vector<uint8_t> m_nonEmptyFlags;
};
