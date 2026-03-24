#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class MotionBlurExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};
