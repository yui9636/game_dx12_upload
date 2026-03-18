#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class BloomExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};