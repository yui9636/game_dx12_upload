#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class GridRenderSystem {
public:
    void Render(Registry& registry, RenderContext& rc);
};