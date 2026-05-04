#include "BattleFlowSystem.h"

#include "Component/TransformComponent.h"
#include "GameLoop/FlowEventQueue.h"
#include "Gameplay/BattleFlowComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/StageBoundsComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <DirectXMath.h>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Phase = BattleFlowComponent::Phase;

    struct BattleFlowRuntime
    {
        Phase phase = Phase::Idle;
        float phaseTimer = 0.0f;
        EntityID playerEntity = Entity::NULL_ID;
        EntityID bossEntity = Entity::NULL_ID;
        EntityID arenaEntity = Entity::NULL_ID;
        float encounterRadius = 18.0f;
        float introDuration = 1.5f;
        std::string battleId = "default";
        bool active = false;
        bool resultEventEmitted = false;
    };

    BattleFlowRuntime g_runtime;
    std::vector<FlowEvent> g_pendingEvents;

    BattleFlowComponent* FindFlow(Registry& registry)
    {
        BattleFlowComponent* found = nullptr;
        Query<BattleFlowComponent> q(registry);
        q.ForEach([&](BattleFlowComponent& flow) {
            if (!found) found = &flow;
        });
        return found;
    }

    void PushBattleEvent(const std::string& name, const std::string& value)
    {
        g_pendingEvents.push_back({ name, value });
    }

    void SyncAuthoringConfig(Registry& registry, BattleFlowComponent* flow)
    {
        if (!flow) return;

        if (!Entity::IsNull(flow->playerEntity) && registry.IsAlive(flow->playerEntity)) {
            g_runtime.playerEntity = flow->playerEntity;
        }
        if (!Entity::IsNull(flow->bossEntity) && registry.IsAlive(flow->bossEntity)) {
            g_runtime.bossEntity = flow->bossEntity;
        }
        if (!Entity::IsNull(flow->arenaEntity) && registry.IsAlive(flow->arenaEntity)) {
            g_runtime.arenaEntity = flow->arenaEntity;
        }
        g_runtime.encounterRadius = flow->encounterRadius;
        g_runtime.introDuration = flow->introDuration;
        if (!flow->battleId.empty()) {
            g_runtime.battleId = flow->battleId;
        }
        if (flow->autoStartOnPlayerEnter && !g_runtime.active) {
            g_runtime.active = true;
        }
    }

    void MirrorRuntimeToComponent(BattleFlowComponent* flow)
    {
        if (!flow) return;
        flow->phase = g_runtime.phase;
        flow->phaseTimer = g_runtime.phaseTimer;
        flow->playerEntity = g_runtime.playerEntity;
        flow->bossEntity = g_runtime.bossEntity;
        flow->arenaEntity = g_runtime.arenaEntity;
        flow->encounterRadius = g_runtime.encounterRadius;
        flow->introDuration = g_runtime.introDuration;
        flow->battleId = g_runtime.battleId;
    }

    void AutoBindEntities(Registry& registry)
    {
        auto resolveOrPick = [&](EntityID& slot, auto&& pickFn) {
            if (!Entity::IsNull(slot) && registry.IsAlive(slot)) return;
            slot = Entity::NULL_ID;
            pickFn(slot);
        };

        resolveOrPick(g_runtime.playerEntity, [&](EntityID& out) {
            Query<PlayerTagComponent, TransformComponent> q(registry);
            q.ForEachWithEntity([&](EntityID e, PlayerTagComponent&, TransformComponent&) {
                if (Entity::IsNull(out)) out = e;
            });
        });
        resolveOrPick(g_runtime.bossEntity, [&](EntityID& out) {
            Query<EnemyTagComponent, TransformComponent> q(registry);
            q.ForEachWithEntity([&](EntityID e, EnemyTagComponent&, TransformComponent&) {
                if (Entity::IsNull(out)) out = e;
            });
        });
        resolveOrPick(g_runtime.arenaEntity, [&](EntityID& out) {
            Query<StageBoundsComponent, TransformComponent> q(registry);
            q.ForEachWithEntity([&](EntityID e, StageBoundsComponent&, TransformComponent&) {
                if (Entity::IsNull(out)) out = e;
            });
        });
    }

    bool PlayerInsideArena(Registry& registry)
    {
        auto* playerTr = registry.GetComponent<TransformComponent>(g_runtime.playerEntity);
        if (!playerTr) return false;

        if (Entity::IsNull(g_runtime.arenaEntity)) {
            auto* bossTr = registry.GetComponent<TransformComponent>(g_runtime.bossEntity);
            if (!bossTr) return false;
            const float dx = playerTr->worldPosition.x - bossTr->worldPosition.x;
            const float dz = playerTr->worldPosition.z - bossTr->worldPosition.z;
            return (dx * dx + dz * dz) <= (g_runtime.encounterRadius * g_runtime.encounterRadius);
        }

        auto* arenaTr = registry.GetComponent<TransformComponent>(g_runtime.arenaEntity);
        auto* bounds = registry.GetComponent<StageBoundsComponent>(g_runtime.arenaEntity);
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

    void SetPhase(Phase phase)
    {
        if (g_runtime.phase == phase) return;
        g_runtime.phase = phase;
        g_runtime.phaseTimer = 0.0f;
        if (phase != Phase::Victory && phase != Phase::Defeat) {
            g_runtime.resultEventEmitted = false;
        }
    }

    void EmitResultIfNeeded(const char* result)
    {
        if (g_runtime.resultEventEmitted) return;
        g_runtime.resultEventEmitted = true;
        PushBattleEvent("battle.result", result);
        PushBattleEvent("battle.ended", result);
        if (std::string(result) == "Victory") {
            PushBattleEvent("battle.victory", g_runtime.battleId);
        }
        else {
            PushBattleEvent("battle.defeat", g_runtime.battleId);
        }
    }
}

void BattleFlowSystem::Start(const std::string& battleId)
{
    if (!battleId.empty()) {
        g_runtime.battleId = battleId;
    }
    g_runtime.active = true;
    g_runtime.phase = Phase::Idle;
    g_runtime.phaseTimer = 0.0f;
    g_runtime.resultEventEmitted = false;
}

void BattleFlowSystem::Reset()
{
    g_runtime = BattleFlowRuntime{};
    g_pendingEvents.clear();
}

void BattleFlowSystem::DrainEvents(FlowEventQueue& outEvents)
{
    for (const FlowEvent& event : g_pendingEvents) {
        outEvents.Push(event.name, event.value);
    }
    g_pendingEvents.clear();
}

void BattleFlowSystem::Update(Registry& registry, float dt)
{
    BattleFlowComponent* flow = FindFlow(registry);
    SyncAuthoringConfig(registry, flow);

    if (!g_runtime.active) {
        MirrorRuntimeToComponent(flow);
        return;
    }

    g_runtime.phaseTimer += dt;
    AutoBindEntities(registry);

    switch (g_runtime.phase) {
    case Phase::Idle: {
        if (Entity::IsNull(g_runtime.playerEntity) || Entity::IsNull(g_runtime.bossEntity)) break;
        if (PlayerInsideArena(registry)) {
            SetPhase(Phase::Encounter);
        }
        break;
    }
    case Phase::Encounter: {
        if (g_runtime.phaseTimer >= g_runtime.introDuration) {
            SetPhase(Phase::Combat);
        }
        break;
    }
    case Phase::Combat: {
        if (IsDead(registry, g_runtime.bossEntity)) {
            SetPhase(Phase::Victory);
        }
        else if (IsDead(registry, g_runtime.playerEntity)) {
            SetPhase(Phase::Defeat);
        }
        break;
    }
    case Phase::Victory:
        EmitResultIfNeeded("Victory");
        break;
    case Phase::Defeat:
        EmitResultIfNeeded("Defeat");
        break;
    }

    MirrorRuntimeToComponent(flow);
}
