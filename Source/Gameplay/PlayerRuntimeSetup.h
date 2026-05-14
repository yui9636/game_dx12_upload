#pragma once

#include "Entity/Entity.h"
// Registry はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class Registry;

namespace PlayerRuntimeSetup
{
    void EnsurePlayerPersistentComponents(Registry& registry, EntityID entity);
    void EnsurePlayerRuntimeComponents(Registry& registry, EntityID entity);
    void ResetPlayerRuntimeState(Registry& registry, EntityID entity);
    void EnsureAllPlayerRuntimeComponents(Registry& registry, bool resetRuntimeState);
    bool HasMinimumPlayerAuthoringComponents(Registry& registry, EntityID entity);
}
