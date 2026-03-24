#pragma once
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include <Engine\EngineTime.h>

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
