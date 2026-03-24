#pragma once
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "RenderGraph/FrameGraphBuilder.h"

class FrameGraphResources;

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    virtual std::string GetName() const { return "UnknownPass"; }

    virtual void Setup(FrameGraphBuilder& builder) {}

    virtual void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) = 0;

    virtual bool HasSideEffects() const { return false; }
};
