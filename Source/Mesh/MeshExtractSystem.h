#pragma once
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "RenderContext/RenderQueue.h"
#include "Component/MaterialComponent.h"

class MeshExtractSystem {
public:
    void Extract(Registry& registry, RenderQueue& queue);
};
