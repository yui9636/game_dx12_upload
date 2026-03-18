#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class ColorFilterExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};