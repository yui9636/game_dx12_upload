#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class EnvironmentExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};
