#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class ShadowExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};
