#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

class DoFExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};