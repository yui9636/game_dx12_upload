#pragma once
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "RenderGraph/FrameGraphBuilder.h"

class FrameGraphResources;

// すべての描画パスの共通インターフェース
class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    virtual std::string GetName() const { return "UnknownPass"; }

    virtual void Setup(FrameGraphBuilder& builder) {}

    virtual void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) = 0;

    // カリング: true を返すパスは依存がなくても必ず実行される
    virtual bool HasSideEffects() const { return false; }
};
