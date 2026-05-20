// IRenderPass のレンダーパス宣言をまとめます。
#pragma once
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "RenderGraph/FrameGraphBuilder.h"

class FrameGraphResources;

// FrameGraph から呼ばれる描画 pass の共通インターフェース。
// Setup で resource の read/write を宣言し、Execute で実際の描画や Context 更新を行う。
class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    // profiler / dump 用の pass 名。
    virtual std::string GetName() const { return "UnknownPass"; }

    // FrameGraphBuilder 経由で依存 resource を登録する。
    virtual void Setup(FrameGraphBuilder& builder, const RenderContext& rc) {}

    // 実体化済み resource と RenderQueue を使って pass 本体を実行する。
    virtual void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) = 0;

    // true の pass は出力 texture だけで判断して cull されない。
    virtual bool HasSideEffects() const { return false; }
};
