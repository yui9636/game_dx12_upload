#pragma once

#include <DirectXMath.h>

#include "Entity/Entity.h"

class Registry;
struct EnemyConfigAsset;

namespace EnemyRuntimeSetup
{
    // Ensure all enemy entities have the components needed for AI to drive them.
    // Symmetric to PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents.
    void EnsureAllEnemyRuntimeComponents(Registry& registry, bool resetRuntimeState);

    // Add the components for one enemy entity.
    void EnsureEnemyRuntimeComponents(Registry& registry, EntityID entity);

    // Reset BT runtime + Aggro state for one enemy entity.
    void ResetEnemyRuntimeState(Registry& registry, EntityID entity);

    // Spawn one enemy entity from a config bundle. Returns NULL_ID on failure.
    EntityID SpawnFromConfig(Registry& registry,
                             const EnemyConfigAsset& config,
                             const DirectX::XMFLOAT3& position);
}
