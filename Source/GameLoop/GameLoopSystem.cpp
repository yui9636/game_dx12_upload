#include "GameLoopSystem.h"

#include <cmath>

#include "Component/ActorTypeComponent.h"
#include "Component/TransformComponent.h"
#include "Engine/EngineKernel.h"
#include "Engine/EngineMode.h"
#include "GameLoopAsset.h"
#include "GameLoopRuntime.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "UIButtonClickEventQueue.h"

namespace
{
    bool IsActionPressedOnOwner(Registry& gameLoopRegistry, int actionIndex)
    {
        if (actionIndex < 0 || actionIndex >= ResolvedInputStateComponent::MAX_ACTIONS) {
            return false;
        }
        bool pressed = false;
        Query<ResolvedInputStateComponent> q(gameLoopRegistry);
        q.ForEach([&](ResolvedInputStateComponent& state) {
            if (actionIndex < state.actionCount && state.actions[actionIndex].pressed) {
                pressed = true;
            }
        });
        return pressed;
    }

    int CountAliveOfType(Registry& reg, ActorType target, int& outDeadCount)
    {
        int total = 0;
        outDeadCount = 0;
        Query<ActorTypeComponent, HealthComponent> q(reg);
        q.ForEach([&](ActorTypeComponent& at, HealthComponent& h) {
            if (at.type != target) return;
            ++total;
            if (h.isDead || h.health <= 0) {
                ++outDeadCount;
            }
        });
        return total;
    }

    bool TryGetFirstActorPosition(Registry& reg, ActorType target, DirectX::XMFLOAT3& outPos)
    {
        bool found = false;
        Query<ActorTypeComponent, TransformComponent> q(reg);
        q.ForEach([&](ActorTypeComponent& at, TransformComponent& tr) {
            if (found)            return;
            if (at.type != target) return;
            outPos = tr.worldPosition;
            found = true;
        });
        return found;
    }

    void InitializeObservedActorIfNeeded(GameLoopRuntime& runtime, Registry& gameRegistry)
    {
        if (runtime.observedActorPositionInitialized) return;

        DirectX::XMFLOAT3 pos{};
        if (TryGetFirstActorPosition(gameRegistry, ActorType::Player, pos)) {
            runtime.observedActorStartPosition = pos;
            runtime.observedActorPositionInitialized = true;
        }
    }

    bool EvaluateCondition(
        const GameLoopCondition&        c,
        GameLoopRuntime&                runtime,
        Registry&                       gameRegistry,
        Registry&                       gameLoopRegistry,
        const UIButtonClickEventQueue&  clickQueue)
    {
        switch (c.type) {
        case GameLoopConditionType::None:
            return true;

        case GameLoopConditionType::InputPressed:
            return IsActionPressedOnOwner(gameLoopRegistry, c.actionIndex);

        case GameLoopConditionType::UIButtonClicked:
            return clickQueue.Contains(c.targetName);

        case GameLoopConditionType::TimerElapsed:
            return runtime.nodeTimer >= c.seconds;

        case GameLoopConditionType::ActorDead:
        {
            int dead = 0;
            const int total = CountAliveOfType(gameRegistry, c.actorType, dead);
            (void)total;
            return dead >= 1;
        }

        case GameLoopConditionType::AllActorsDead:
        {
            int dead = 0;
            const int total = CountAliveOfType(gameRegistry, c.actorType, dead);
            return total > 0 && dead >= total;
        }

        case GameLoopConditionType::ActorMovedDistance:
        {
            DirectX::XMFLOAT3 cur{};
            if (!TryGetFirstActorPosition(gameRegistry, c.actorType, cur)) return false;

            // First chance: capture the start position.
            if (!runtime.observedActorPositionInitialized) {
                runtime.observedActorStartPosition = cur;
                runtime.observedActorPositionInitialized = true;
                return false;
            }

            const float dx = cur.x - runtime.observedActorStartPosition.x;
            const float dz = cur.z - runtime.observedActorStartPosition.z;
            const float dist = std::sqrt(dx * dx + dz * dz);
            return dist >= c.threshold;
        }

        case GameLoopConditionType::RuntimeFlag:
        case GameLoopConditionType::CustomEvent:
        {
            const std::string& key = (c.type == GameLoopConditionType::RuntimeFlag)
                ? c.parameterName : c.eventName;
            auto it = runtime.flags.find(key);
            return it != runtime.flags.end() && it->second;
        }

        case GameLoopConditionType::StateMachineState:
        {
            // Match: actor of type c.actorType whose current state name == c.parameterName.
            if (c.parameterName.empty()) return false;
            bool matched = false;
            Query<ActorTypeComponent, StateMachineAssetComponent, StateMachineParamsComponent> q(gameRegistry);
            q.ForEach([&](ActorTypeComponent& at, StateMachineAssetComponent& asset, StateMachineParamsComponent& params) {
                if (matched) return;
                if (c.actorType != ActorType::None && at.type != c.actorType) return;
                for (const auto& s : asset.asset.states) {
                    if (s.id == params.currentStateId && s.name == c.parameterName) {
                        matched = true;
                        return;
                    }
                }
            });
            return matched;
        }

        case GameLoopConditionType::TimelineEvent:
        {
            // Gameplay code is expected to set runtime.flags[eventName] = true.
            auto it = runtime.flags.find(c.eventName);
            return it != runtime.flags.end() && it->second;
        }

        default:
            return false;
        }
    }

    bool EvaluateTransition(
        const GameLoopTransition&       transition,
        GameLoopRuntime&                runtime,
        Registry&                       gameRegistry,
        Registry&                       gameLoopRegistry,
        const UIButtonClickEventQueue&  clickQueue)
    {
        if (transition.conditions.empty()) return false;

        if (transition.requireAllConditions) {
            for (const auto& c : transition.conditions) {
                if (!EvaluateCondition(c, runtime, gameRegistry, gameLoopRegistry, clickQueue)) return false;
            }
            return true;
        }
        for (const auto& c : transition.conditions) {
            if (EvaluateCondition(c, runtime, gameRegistry, gameLoopRegistry, clickQueue)) return true;
        }
        return false;
    }
}

void GameLoopSystem::Update(
    const GameLoopAsset&           asset,
    GameLoopRuntime&               runtime,
    Registry&                      gameRegistry,
    Registry&                      gameLoopRegistry,
    const UIButtonClickEventQueue& clickQueue,
    float                          dt)
{
    if (!runtime.isActive)                                              return;
    if (EngineKernel::Instance().GetMode() != EngineMode::Play)         return;
    if (runtime.waitingSceneLoad)                                       return;
    if (runtime.sceneTransitionRequested)                               return;
    if (runtime.currentNodeId == 0)                                     return;

    runtime.nodeTimer += dt;

    InitializeObservedActorIfNeeded(runtime, gameRegistry);

    for (const auto& transition : asset.transitions) {
        if (transition.fromNodeId != runtime.currentNodeId) continue;
        if (!EvaluateTransition(transition, runtime, gameRegistry, gameLoopRegistry, clickQueue))
            continue;

        const GameLoopNode* toNode = asset.FindNode(transition.toNodeId);
        if (!toNode) return;

        runtime.pendingNodeId             = toNode->id;
        runtime.pendingScenePath          = toNode->scenePath;
        runtime.sceneTransitionRequested  = true;
        runtime.forceReload               = (toNode->id == runtime.currentNodeId);
        return;
    }
}
