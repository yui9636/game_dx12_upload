#pragma once

#include <DirectXMath.h>

#include "Entity/Entity.h"

class Registry;
struct EnemyConfigAsset;

struct StateMachineAsset;

namespace EnemyRuntimeSetup
{
    // Ensure all enemy entities have the components needed for AI to drive them.
    // Symmetric to PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents.
    void EnsureAllEnemyRuntimeComponents(Registry& registry, bool resetRuntimeState);

    // Add the components for one enemy entity.
    void EnsureEnemyRuntimeComponents(Registry& registry, EntityID entity);

    // Add minimal NPC components (Locomotion + ActionState + StateMachine,
    // no AI / Perception / Aggro). Used by Setup Full NPC.
    void EnsureNPCRuntimeComponents(Registry& registry, EntityID entity);

    // Reset BT runtime + Aggro state for one enemy entity.
    void ResetEnemyRuntimeState(Registry& registry, EntityID entity);

    // Spawn one enemy entity from a config bundle. Returns NULL_ID on failure.
    EntityID SpawnFromConfig(Registry& registry,
                             const EnemyConfigAsset& config,
                             const DirectX::XMFLOAT3& position);
}

// Free helpers used by PlayerEditorPanel toolbar buttons (v2.0 ActorEditor).
// Declared at namespace scope so PlayerEditor can reach them via extern decl.
void EnemyEditorSetupFullEnemy(Registry& registry, EntityID entity, StateMachineAsset& sm);
void EnemyEditorSetupFullNPC  (Registry& registry, EntityID entity, StateMachineAsset& sm);
void EnemyEditorRepairRuntime (Registry& registry, EntityID entity);
