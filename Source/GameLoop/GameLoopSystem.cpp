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

    bool HasLoadSceneAction(const GameLoopTransition& transition)
    {
        for (const GameFlowAction& action : transition.actions) {
            if (action.type == GameFlowActionType::LoadScene) {
                return true;
            }
        }
        return false;
    }

    void EnqueueLoadSceneAction(
        GameLoopRuntime& runtime,
        const GameLoopNode& toNode,
        const std::string& scenePath)
    {
        if (scenePath.empty()) {
            return;
        }

        GameFlowAction action;
        action.type = GameFlowActionType::LoadScene;
        action.target = scenePath;
        runtime.pendingActions.push_back({ action, toNode.id });
    }

    void EnqueueWaitAction(GameLoopRuntime& runtime, const GameLoopNode& toNode, float seconds)
    {
        if (seconds <= 0.0f) {
            return;
        }

        GameFlowAction action;
        action.type = GameFlowActionType::Wait;
        action.seconds = seconds;
        runtime.pendingActions.push_back({ action, toNode.id });
    }

    enum class ActionResult
    {
        Continue,
        Blocked,
        SceneRequested,
    };

    ActionResult ExecuteAction(
        const GameFlowAction& action,
        const GameLoopNode& toNode,
        GameLoopRuntime& runtime,
        FlowEventQueue& flowEvents)
    {
        switch (action.type) {
        case GameFlowActionType::LoadScene:
            RequestSceneTransition(runtime, toNode, action.target);
            return runtime.sceneTransitionRequested ? ActionResult::SceneRequested : ActionResult::Continue;

        case GameFlowActionType::SetCurrentNode:
            runtime.previousNodeId = runtime.currentNodeId;
            runtime.currentNodeId = toNode.id;
            runtime.nodeTimer = 0.0f;
            return ActionResult::Continue;

        case GameFlowActionType::EmitEvent:
            flowEvents.Push(action.target, action.value);
            return ActionResult::Continue;

        case GameFlowActionType::SetFlag:
            if (!action.target.empty()) runtime.flags[action.target] = action.boolValue;
            flowEvents.Push("flow.flag.changed", action.target);
            return ActionResult::Continue;

        case GameFlowActionType::ClearFlag:
            if (!action.target.empty()) runtime.flags[action.target] = false;
            flowEvents.Push("flow.flag.changed", action.target);
            return ActionResult::Continue;

        case GameFlowActionType::StartBattleFlow:
            BattleFlowSystem::Start(action.target);
            flowEvents.Push("battle.start.requested", action.target);
            return ActionResult::Continue;

        case GameFlowActionType::ResetBattleFlow:
            BattleFlowSystem::Reset();
            flowEvents.Push("battle.reset.requested", action.target);
            return ActionResult::Continue;

        case GameFlowActionType::Fade:
            runtime.fadeDuration = (std::max)(0.0f, action.seconds);
            runtime.fadeRemaining = runtime.fadeDuration;
            runtime.fadeAlpha = runtime.fadeDuration > 0.0f ? 1.0f : 0.0f;
            flowEvents.Push("flow.fade.started", action.target);
            if (runtime.fadeRemaining > 0.0f) {
                return ActionResult::Blocked;
            }
            return ActionResult::Continue;

        case GameFlowActionType::Wait:
            runtime.actionWaitRemaining = (std::max)(0.0f, action.seconds);
            flowEvents.Push("flow.wait.started", action.target);
            if (runtime.actionWaitRemaining > 0.0f) {
                return ActionResult::Blocked;
            }
            return ActionResult::Continue;

        case GameFlowActionType::ShowLoadingOverlay:
            runtime.loadingOverlayVisible = true;
            runtime.loadingMessage = !action.message.empty() ? action.message :
                (!action.value.empty() ? action.value : std::string{ "Loading..." });
            flowEvents.Push("loading.overlay.shown", runtime.loadingMessage);
            return ActionResult::Continue;

        case GameFlowActionType::HideLoadingOverlay:
            runtime.loadingOverlayVisible = false;
            runtime.loadingMessage.clear();
            flowEvents.Push("loading.overlay.hidden", action.target);
            return ActionResult::Continue;
        }

        return ActionResult::Continue;
    }

    void EnqueueTransitionActions(
        const GameLoopTransition& transition,
        const GameLoopNode& toNode,
        GameLoopRuntime& runtime)
    {
        if (!transition.loadingScenePath.empty()) {
            EnqueueLoadSceneAction(runtime, toNode, transition.loadingScenePath);
            EnqueueWaitAction(runtime, toNode, transition.loadingMinimumSeconds);
        }

        if (transition.actions.empty()) {
            if (toNode.type == GameLoopNodeType::Scene && !toNode.scenePath.empty()) {
                EnqueueLoadSceneAction(runtime, toNode, toNode.scenePath);
            }
            else {
                GameFlowAction action;
                action.type = GameFlowActionType::SetCurrentNode;
                runtime.pendingActions.push_back({ action, toNode.id });
            }
            return;
        }

        if (toNode.type == GameLoopNodeType::Scene &&
            !toNode.scenePath.empty() &&
            !HasLoadSceneAction(transition)) {
            EnqueueLoadSceneAction(runtime, toNode, toNode.scenePath);
        }

        for (const GameFlowAction& action : transition.actions) {
            GameFlowAction queuedAction = action;
            if (queuedAction.type == GameFlowActionType::LoadScene &&
                toNode.type == GameLoopNodeType::Scene &&
                !toNode.scenePath.empty()) {
                queuedAction.target = toNode.scenePath;
            }
            runtime.pendingActions.push_back({ queuedAction, toNode.id });
        }
    }

    ActionResult ProcessQueuedActions(
        const GameLoopAsset& asset,
        GameLoopRuntime& runtime,
        FlowEventQueue& flowEvents,
        float dt)
    {
        if (runtime.actionWaitRemaining > 0.0f) {
            runtime.actionWaitRemaining = (std::max)(0.0f, runtime.actionWaitRemaining - dt);
            return ActionResult::Blocked;
        }

        if (runtime.fadeRemaining > 0.0f) {
            runtime.fadeRemaining = (std::max)(0.0f, runtime.fadeRemaining - dt);
            if (runtime.fadeDuration > 0.0f) {
                runtime.fadeAlpha = runtime.fadeRemaining / runtime.fadeDuration;
            }
            else {
                runtime.fadeAlpha = 0.0f;
            }
            return ActionResult::Blocked;
        }

        while (!runtime.pendingActions.empty()) {
            QueuedGameFlowAction queued = runtime.pendingActions.front();
            runtime.pendingActions.erase(runtime.pendingActions.begin());

            const GameLoopNode* toNode = asset.FindNode(queued.toNodeId);
            if (!toNode) {
                continue;
            }

            const ActionResult result = ExecuteAction(queued.action, *toNode, runtime, flowEvents);
            if (result != ActionResult::Continue) {
                return result;
            }
        }

        return ActionResult::Continue;
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

    if (ProcessQueuedActions(asset, runtime, flowEvents, dt) != ActionResult::Continue) {
        return;
    }

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

    EnqueueTransitionActions(*selectedTransition, *toNode, runtime);
    ProcessQueuedActions(asset, runtime, flowEvents, dt);
}
