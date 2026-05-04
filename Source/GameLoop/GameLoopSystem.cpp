#include "GameLoopSystem.h"

#include <algorithm>
#include <string>

#include "Engine/EngineKernel.h"
#include "Engine/EngineMode.h"
#include "FlowEventQueue.h"
#include "GameLoopAsset.h"
#include "GameLoopRuntime.h"
#include "Gameplay/BattleFlowSystem.h"
#include "Registry/Registry.h"

namespace
{
    std::string ToString(uint32_t value)
    {
        return std::to_string(value);
    }

    std::string ToString(uint8_t value)
    {
        return std::to_string(static_cast<unsigned int>(value));
    }

    bool IsFlagSet(const GameLoopRuntime& runtime, const std::string& name)
    {
        const auto it = runtime.flags.find(name);
        return it != runtime.flags.end() && it->second;
    }

    bool IsConditionSatisfied(
        const GameFlowCondition& condition,
        const GameLoopRuntime& runtime,
        const FlowEventQueue& flowEvents)
    {
        switch (condition.type) {
        case GameFlowConditionType::Event:
            return flowEvents.Contains(condition.name, condition.value);

        case GameFlowConditionType::InputAction:
            if (condition.keyboardScancode != 0 &&
                flowEvents.Contains("input.keyboard.down", ToString(condition.keyboardScancode))) {
                return true;
            }
            if (condition.gamepadButton != kGameFlowUnboundGamepadButton &&
                flowEvents.Contains("input.gamepad.down", ToString(condition.gamepadButton))) {
                return true;
            }
            if (!condition.value.empty() &&
                flowEvents.Contains("input.action.pressed", condition.value)) {
                return true;
            }
            return false;

        case GameFlowConditionType::UIButtonClick:
            return flowEvents.Contains("ui.button.clicked", condition.value);

        case GameFlowConditionType::TimerElapsed:
            return runtime.nodeTimer >= condition.seconds;

        case GameFlowConditionType::FlagEquals:
            return IsFlagSet(runtime, condition.name) == condition.expectedFlagValue;

        case GameFlowConditionType::BattleResult:
            return flowEvents.Contains("battle.ended", condition.value) ||
                flowEvents.Contains("battle.result", condition.value);

        case GameFlowConditionType::SceneLoaded:
            return flowEvents.Contains("scene.loaded", condition.value);
        }

        return false;
    }

    bool AreTransitionConditionsSatisfied(
        const GameLoopTransition& transition,
        const GameLoopRuntime& runtime,
        const FlowEventQueue& flowEvents)
    {
        if (transition.conditions.empty()) return false;

        if (transition.conditionMode == GameFlowConditionMode::Any) {
            for (const GameFlowCondition& condition : transition.conditions) {
                if (IsConditionSatisfied(condition, runtime, flowEvents)) return true;
            }
            return false;
        }

        for (const GameFlowCondition& condition : transition.conditions) {
            if (!IsConditionSatisfied(condition, runtime, flowEvents)) return false;
        }
        return true;
    }

    void RequestSceneTransition(
        GameLoopRuntime& runtime,
        const GameLoopNode& toNode,
        const std::string& explicitScenePath)
    {
        const std::string scenePath = explicitScenePath.empty() ? toNode.scenePath : explicitScenePath;
        if (scenePath.empty()) return;

        runtime.pendingNodeId = toNode.id;
        runtime.pendingScenePath = scenePath;
        runtime.sceneTransitionRequested = true;
        runtime.forceReload = (toNode.id == runtime.currentNodeId);
    }

    bool ExecuteAction(
        const GameFlowAction& action,
        const GameLoopNode& toNode,
        GameLoopRuntime& runtime,
        FlowEventQueue& flowEvents)
    {
        switch (action.type) {
        case GameFlowActionType::LoadScene:
            RequestSceneTransition(runtime, toNode, action.target);
            return runtime.sceneTransitionRequested;

        case GameFlowActionType::SetCurrentNode:
            runtime.previousNodeId = runtime.currentNodeId;
            runtime.currentNodeId = toNode.id;
            runtime.nodeTimer = 0.0f;
            return false;

        case GameFlowActionType::EmitEvent:
            flowEvents.Push(action.target, action.value);
            return false;

        case GameFlowActionType::SetFlag:
            if (!action.target.empty()) runtime.flags[action.target] = action.boolValue;
            return false;

        case GameFlowActionType::ClearFlag:
            if (!action.target.empty()) runtime.flags[action.target] = false;
            return false;

        case GameFlowActionType::StartBattleFlow:
            BattleFlowSystem::Start(action.target);
            return false;

        case GameFlowActionType::ResetBattleFlow:
            BattleFlowSystem::Reset();
            return false;

        case GameFlowActionType::Fade:
        case GameFlowActionType::Wait:
            return false;
        }

        return false;
    }
}

void GameLoopSystem::Update(
    const GameLoopAsset& asset,
    GameLoopRuntime&     runtime,
    Registry&            gameRegistry,
    Registry&            gameLoopRegistry,
    FlowEventQueue&      flowEvents,
    float                dt)
{
    (void)gameRegistry;
    (void)gameLoopRegistry;

    if (!runtime.isActive) return;
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) return;
    if (runtime.waitingSceneLoad) return;
    if (runtime.sceneTransitionRequested) return;
    if (runtime.currentNodeId == 0) return;

    runtime.nodeTimer += dt;

    const GameLoopTransition* selectedTransition = nullptr;
    for (const GameLoopTransition& transition : asset.transitions) {
        if (transition.fromNodeId != runtime.currentNodeId) continue;
        if (!AreTransitionConditionsSatisfied(transition, runtime, flowEvents)) continue;

        if (!selectedTransition || transition.priority > selectedTransition->priority) {
            selectedTransition = &transition;
        }
    }

    if (!selectedTransition) return;

    const GameLoopNode* toNode = asset.FindNode(selectedTransition->toNodeId);
    if (!toNode) return;

    bool requestedScene = false;
    for (const GameFlowAction& action : selectedTransition->actions) {
        requestedScene = ExecuteAction(action, *toNode, runtime, flowEvents) || requestedScene;
    }

    if (!requestedScene && !toNode->scenePath.empty()) {
        RequestSceneTransition(runtime, *toNode, std::string{});
    }
}
