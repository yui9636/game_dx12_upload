#pragma once
// Registry はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Registry;
class RenderQueue;
struct RenderContext;

class TrailExtractSystem
{
public:
    static void Extract(Registry& registry, RenderQueue& queue, const RenderContext& rc);
};
