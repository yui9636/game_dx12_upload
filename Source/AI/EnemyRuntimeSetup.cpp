#include "EnemyRuntimeSetup.h"

#include "Archetype/Archetype.h"
#include "Component/ActorTypeComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/ComponentSignature.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/CharacterPhysicsComponent.h"
#include "Gameplay/DodgeStateComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HitStopComponent.h"
#include "Gameplay/HitboxTrackingComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

#include "AggroComponent.h"
#include "BehaviorTreeAssetComponent.h"
#include "BehaviorTreeRuntimeComponent.h"
#include "BlackboardComponent.h"
#include "EnemyConfigAsset.h"
#include "PerceptionComponent.h"

namespace
{
    template<typename T>
    T* EnsureComponent(Registry& registry, EntityID entity)
    {
        if (auto* c = registry.GetComponent<T>(entity)) return c;
        registry.AddComponent(entity, T{});
        return registry.GetComponent<T>(entity);
    }
}

namespace EnemyRuntimeSetup
{
    void EnsureEnemyRuntimeComponents(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity)) return;

        EnsureComponent<EnemyTagComponent>(registry, entity);

        if (auto* actor = EnsureComponent<ActorTypeComponent>(registry, entity)) {
            actor->type = ActorType::Enemy;
        }

        EnsureComponent<TransformComponent>(registry, entity);
        EnsureComponent<HierarchyComponent>(registry, entity);
        EnsureComponent<HealthComponent>(registry, entity);
        EnsureComponent<CharacterPhysicsComponent>(registry, entity);
        EnsureComponent<HitStopComponent>(registry, entity);
        EnsureComponent<HitboxTrackingComponent>(registry, entity);
        EnsureComponent<TimelineComponent>(registry, entity);
        EnsureComponent<TimelineItemBuffer>(registry, entity);
        EnsureComponent<ActionStateComponent>(registry, entity);
        EnsureComponent<DodgeStateComponent>(registry, entity);
        EnsureComponent<StateMachineParamsComponent>(registry, entity);

        if (auto* loco = EnsureComponent<LocomotionStateComponent>(registry, entity)) {
            // AI writes world-space x/z directly; bypass camera transform.
            loco->useCameraRelativeInput = false;
        }

        EnsureComponent<BehaviorTreeAssetComponent>(registry, entity);
        EnsureComponent<BehaviorTreeRuntimeComponent>(registry, entity);
        EnsureComponent<BlackboardComponent>(registry, entity);
        EnsureComponent<PerceptionComponent>(registry, entity);
        EnsureComponent<AggroComponent>(registry, entity);
    }

    void ResetEnemyRuntimeState(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity)) return;
        if (auto* rt = registry.GetComponent<BehaviorTreeRuntimeComponent>(entity)) {
            rt->ResetAll();
        }
        if (auto* aggro = registry.GetComponent<AggroComponent>(entity)) {
            aggro->currentTarget   = Entity::NULL_ID;
            aggro->threat          = 0.0f;
            aggro->timeSinceSighted = 0.0f;
        }
        if (auto* bb = registry.GetComponent<BlackboardComponent>(entity)) {
            bb->entries.clear();
        }
        if (auto* loco = registry.GetComponent<LocomotionStateComponent>(entity)) {
            loco->moveInput   = { 0.0f, 0.0f };
            loco->inputStrength = 0.0f;
        }
    }

    void EnsureAllEnemyRuntimeComponents(Registry& registry, bool resetRuntimeState)
    {
        // Sweep: any entity tagged Enemy gets the full enemy component set.
        // Use snapshot (avoid mutation during traversal).
        Signature sig = CreateSignature<EnemyTagComponent>();
        std::vector<EntityID> enemies;
        for (auto* arch : registry.GetAllArchetypes()) {
            if (!SignatureMatches(arch->GetSignature(), sig)) continue;
            const auto& ents = arch->GetEntities();
            for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                enemies.push_back(ents[i]);
            }
        }
        for (EntityID e : enemies) {
            EnsureEnemyRuntimeComponents(registry, e);
            if (resetRuntimeState) ResetEnemyRuntimeState(registry, e);
        }
    }

    EntityID SpawnFromConfig(Registry& registry,
                             const EnemyConfigAsset& config,
                             const DirectX::XMFLOAT3& position)
    {
        EntityID e = registry.CreateEntity();
        registry.AddComponent(e, NameComponent{ config.name.empty() ? "Enemy" : config.name });

        EnsureEnemyRuntimeComponents(registry, e);

        if (auto* tr = registry.GetComponent<TransformComponent>(e)) {
            tr->localPosition = position;
            tr->worldPosition = position;
        }

        if (auto* health = registry.GetComponent<HealthComponent>(e)) {
            health->maxHealth = static_cast<int>(config.maxHealth);
            health->health    = health->maxHealth;
        }

        if (auto* loco = registry.GetComponent<LocomotionStateComponent>(e)) {
            loco->walkMaxSpeed = config.walkSpeed;
            loco->jogMaxSpeed  = (config.walkSpeed + config.runSpeed) * 0.5f;
            loco->runMaxSpeed  = config.runSpeed;
            loco->turnSpeed    = config.turnSpeed;
        }

        if (auto* perception = registry.GetComponent<PerceptionComponent>(e)) {
            perception->sightRadius   = config.sightRadius;
            perception->sightFOV      = config.sightFOV;
            perception->hearingRadius = config.hearingRadius;
        }

        if (auto* btAsset = registry.GetComponent<BehaviorTreeAssetComponent>(e)) {
            btAsset->assetPath = config.behaviorTreePath;
        }

        return e;
    }
}
