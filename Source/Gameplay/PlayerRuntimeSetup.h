#pragma once

#include "Entity/Entity.h"

class Registry;

namespace PlayerRuntimeSetup
{
    void EnsurePlayerPersistentComponents(Registry& registry, EntityID entity);
    void EnsurePlayerRuntimeComponents(Registry& registry, EntityID entity);
    void ResetPlayerRuntimeState(Registry& registry, EntityID entity);
    bool HasMinimumPlayerAuthoringComponents(Registry& registry, EntityID entity);
}
