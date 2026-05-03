#include "BattleFlowSystem.h"

#include "Component/TransformComponent.h"
#include "Gameplay/BattleFlowComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/StageBoundsComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <DirectXMath.h>
#include <cmath>

namespace
{
    BattleFlowComponent* FindFlow(Registry& registry)
    {
        BattleFlowComponent* found = nullptr;
        Query<BattleFlowComponent> q(registry);
        q.ForEach([&](BattleFlowComponent& flow) {
            if (!found) found = &flow;
        });
        return found;
    }

    void AutoBindEntities(Registry& registry, BattleFlowComponent& flow)
    {
        auto resolveOrPick = [&](EntityID& slot, auto&& pickFn) {
            if (!Entity::IsNull(slot) && registry.IsAlive(slot)) return;
            slot = Entity::NULL_ID;
            pickFn(slot);
        };

        resolveOrPick(flow.playerEntity, [&](EntityID& out) {
            Query<PlayerTagComponent, TransformComponent> q(registry);
            q.ForEachWithEntity([&](EntityID e, PlayerTagComponent&, TransformComponent&) {
                if (Entity::IsNull(out)) out = e;
            });
        });
        resolveOrPick(flow.bossEntity, [&](EntityID& out) {
            Query<EnemyTagComponent, TransformComponent> q(registry);
            q.ForEachWithEntity([&](EntityID e, EnemyTagComponent&, TransformComponent&) {
                if (Entity::IsNull(out)) out = e;
            });
        });
        resolveOrPick(flow.arenaEntity, [&](EntityID& out) {
            Query<StageBoundsComponent, TransformComponent> q(registry);
            q.ForEachWithEntity([&](EntityID e, StageBoundsComponent&, TransformComponent&) {
                if (Entity::IsNull(out)) out = e;
            });
        });
    }

    bool PlayerInsideArena(Registry& registry, const BattleFlowComponent& flow)
    {
        auto* playerTr = registry.GetComponent<TransformComponent>(flow.playerEntity);
        if (!playerTr) return false;

        if (Entity::IsNull(flow.arenaEntity)) {
            // No arena explicitly set: distance-based fallback against the boss.
            auto* bossTr = registry.GetComponent<TransformComponent>(flow.bossEntity);
            if (!bossTr) return false;
            const float dx = playerTr->worldPosition.x - bossTr->worldPosition.x;
            const float dz = playerTr->worldPosition.z - bossTr->worldPosition.z;
            return (dx * dx + dz * dz) <= (flow.encounterRadius * flow.encounterRadius);
        }

        auto* arenaTr = registry.GetComponent<TransformComponent>(flow.arenaEntity);
        auto* bounds = registry.GetComponent<StageBoundsComponent>(flow.arenaEntity);
        if (!arenaTr || !bounds) return false;
        const float dx = playerTr->worldPosition.x - arenaTr->worldPosition.x;
        const float dz = playerTr->worldPosition.z - arenaTr->worldPosition.z;
        return (dx * dx + dz * dz) <= (bounds->radius * bounds->radius);
    }

    bool IsDead(Registry& registry, EntityID e)
    {
        if (Entity::IsNull(e)) return true;
        auto* h = registry.GetComponent<HealthComponent>(e);
        if (!h) return false;
        return h->isDead || h->health <= 0;
    }
}

void BattleFlowSystem::Update(Registry& registry, float dt)
{
    BattleFlowComponent* flow = FindFlow(registry);
    if (!flow) return;

    flow->phaseTimer += dt;
    AutoBindEntities(registry, *flow);

    using Phase = BattleFlowComponent::Phase;

    switch (flow->phase) {
    case Phase::Idle: {
        if (Entity::IsNull(flow->playerEntity) || Entity::IsNull(flow->bossEntity)) break;
        if (PlayerInsideArena(registry, *flow)) {
            flow->phase      = Phase::Encounter;
            flow->phaseTimer = 0.0f;
        }
        break;
    }
    case Phase::Encounter: {
        if (flow->phaseTimer >= flow->introDuration) {
            flow->phase      = Phase::Combat;
            flow->phaseTimer = 0.0f;
        }
        break;
    }
    case Phase::Combat: {
        if (IsDead(registry, flow->bossEntity)) {
            flow->phase      = Phase::Victory;
            flow->phaseTimer = 0.0f;
        } else if (IsDead(registry, flow->playerEntity)) {
            flow->phase      = Phase::Defeat;
            flow->phaseTimer = 0.0f;
        }
        break;
    }
    case Phase::Victory:
    case Phase::Defeat:
        // Outro / restart hooks are handled by upstream UI / GameLoop systems.
        break;
    }
}
