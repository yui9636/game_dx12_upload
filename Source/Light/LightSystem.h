#pragma once
#include "Registry/Registry.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"
#include <RenderContext\RenderContext.h>

// ECS 上の LightComponent を走査して、描画用のライト情報を RenderContext に集めるシステムです。
class LightSystem {
public:
    // Registry 内の LightComponent と TransformComponent を持つエンティティを探し、
    // 点光源と平行光源を RenderContext に反映します。
    static void ExtractLights(Registry& registry, RenderContext& rc);
};
