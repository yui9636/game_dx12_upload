#pragma once
#include "Registry/Registry.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"
#include <RenderContext\RenderContext.h>

class LightSystem {
public:
    static void ExtractLights(Registry& registry, RenderContext& rc);
};