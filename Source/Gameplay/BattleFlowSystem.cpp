// BattleFlowSystem の Gameplay 関連実装をまとめます。
#include "BattleFlowSystem.h"

#include "Component/TransformComponent.h"
#include "Component/BattleRulesComponent.h"
#include "GameLoop/FlowEventQueue.h"
#include "Gameplay/BattleFlowComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/StageBoundsComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include <algorithm>

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

        // Rules read from BattleRulesComponent each frame.
        bool  enableTimeLimit = false;
        float timeLimitSeconds = 99.0f;
        int   timeUpResult = 0;       // 0=higher HP wins, 1=player loses, 2=draw
        bool  autoStartOnPlayerEnter = true;

        // Combat-phase elapsed time, for the time limit.
        float combatTime = 0.0f;
    };

    BattleFlowRuntime g_runtime;
    std::vector<FlowEvent> g_pendingEvents;

    void PushBattleEvent(const std::string& name, const std::string& value)
    {
        g_pendingEvents.push_back({ name, value });
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

    // Health ratio [0,1]. Missing health component counts as full.
    float HealthRatio(Registry& registry, EntityID e)
    {
        auto* h = registry.GetComponent<HealthComponent>(e);
        if (!h || h->maxHealth <= 0) return 1.0f;
        const float r = static_cast<float>(h->health) / static_cast<float>(h->maxHealth);
        return r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
    }

    // Pull the configurable rules from the first BattleRulesComponent in the scene.
    void ReadRules(Registry& registry)
    {
        bool found = false;
        Query<BattleRulesComponent> q(registry);
        q.ForEach([&](BattleRulesComponent& rules) {
            if (found) return;
            found = true;
            g_runtime.encounterRadius = rules.encounterRadius;
            g_runtime.introDuration = rules.introDuration;
            g_runtime.autoStartOnPlayerEnter = rules.autoStartOnPlayerEnter;
            g_runtime.enableTimeLimit = rules.enableTimeLimit;
            g_runtime.timeLimitSeconds = rules.timeLimitSeconds;
            g_runtime.timeUpResult = rules.timeUpResult;
        });
    }

    void SetPhase(Phase phase)
    {
        if (g_runtime.phase == phase) return;
        g_runtime.phase = phase;
        g_runtime.phaseTimer = 0.0f;
        if (phase == Phase::Combat) {
            g_runtime.combatTime = 0.0f;
        }
        if (phase != Phase::Victory && phase != Phase::Defeat && phase != Phase::Draw) {
            g_runtime.resultEventEmitted = false;
        }
    }

    void EmitResultIfNeeded(const char* result)
    {
        if (g_runtime.resultEventEmitted) return;
        g_runtime.resultEventEmitted = true;
        PushBattleEvent("battle.result", result);
        PushBattleEvent("battle.ended", result);
        const std::string r = result;
        if (r == "Victory") {
            PushBattleEvent("battle.victory", g_runtime.battleId);
        }
        else if (r == "Draw") {
            PushBattleEvent("battle.draw", g_runtime.battleId);
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
    if (!g_runtime.active) {
        return;
    }

    g_runtime.phaseTimer += dt;
    AutoBindEntities(registry);
    ReadRules(registry);

    switch (g_runtime.phase) {
    case Phase::Idle: {
        if (Entity::IsNull(g_runtime.playerEntity) || Entity::IsNull(g_runtime.bossEntity)) break;
        if (g_runtime.autoStartOnPlayerEnter && PlayerInsideArena(registry)) {
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
        g_runtime.combatTime += dt;

        // HP-based result has priority over the timer.
        if (IsDead(registry, g_runtime.bossEntity)) {
            SetPhase(Phase::Victory);
            break;
        }
        if (IsDead(registry, g_runtime.playerEntity)) {
            SetPhase(Phase::Defeat);
            break;
        }

        // Time limit.
        if (g_runtime.enableTimeLimit && g_runtime.combatTime >= g_runtime.timeLimitSeconds) {
            if (g_runtime.timeUpResult == 1) {
                SetPhase(Phase::Defeat);
            }
            else if (g_runtime.timeUpResult == 2) {
                SetPhase(Phase::Draw);
            }
            else {
                // Higher HP ratio wins.
                const float playerHp = HealthRatio(registry, g_runtime.playerEntity);
                const float bossHp = HealthRatio(registry, g_runtime.bossEntity);
                if (playerHp > bossHp + 0.0001f) {
                    SetPhase(Phase::Victory);
                }
                else if (bossHp > playerHp + 0.0001f) {
                    SetPhase(Phase::Defeat);
                }
                else {
                    SetPhase(Phase::Draw);
                }
            }
        }
        break;
    }
    case Phase::Victory:
        EmitResultIfNeeded("Victory");
        break;
    case Phase::Defeat:
        EmitResultIfNeeded("Defeat");
        break;
    case Phase::Draw:
        EmitResultIfNeeded("Draw");
        break;
    }

}

float BattleFlowSystem::GetRemainingTime()
{
    if (!g_runtime.active || !g_runtime.enableTimeLimit) {
        return -1.0f;
    }
    const float remaining = g_runtime.timeLimitSeconds - g_runtime.combatTime;
    return remaining > 0.0f ? remaining : 0.0f;
}

int BattleFlowSystem::GetPhase()
{
    return static_cast<int>(g_runtime.phase);
}
