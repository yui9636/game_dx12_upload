#pragma once
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include <Engine\EngineTime.h>
// Layer はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Layer
{
public:
    virtual ~Layer() = default;

    virtual void Initialize() {}
    virtual void Finalize() {}

    virtual void Update(const EngineTime& time) {}

    virtual void Render(RenderContext& rc, RenderQueue& queue) {}

    virtual void RenderUI() {}
};
