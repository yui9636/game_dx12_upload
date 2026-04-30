#include "GameLoopSystem.h"

#include "Engine/EngineKernel.h"
#include "Engine/EngineMode.h"
#include "GameLoopAsset.h"
#include "GameLoopRuntime.h"
#include "Input/InputEventQueue.h"
#include "Registry/Registry.h"
#include "UIButtonClickEventQueue.h"

namespace
{
    bool IsInputPressed(const GameLoopTransitionInput& input, const InputEventQueue& inputQueue)
    {
        if (input.keyboardScancode == 0 && input.gamepadButton == 0xFF) {
            return false;
        }

        const auto& events = inputQueue.GetEvents();
        for (const InputEvent& event : events) {
            if (input.keyboardScancode != 0 &&
                event.type == InputEventType::KeyDown &&
                !event.key.repeat &&
                event.key.scancode == input.keyboardScancode) {
                return true;
            }

            if (input.gamepadButton != 0xFF &&
                event.type == InputEventType::GamepadButtonDown &&
                event.gamepadButton.button == input.gamepadButton) {
                return true;
            }
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
    const InputEventQueue&         inputQueue,
    float                          dt)
{
    (void)gameRegistry;
    (void)gameLoopRegistry;
    (void)clickQueue;

    if (!runtime.isActive) return;
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) return;
    if (runtime.waitingSceneLoad) return;
    if (runtime.sceneTransitionRequested) return;
    if (runtime.currentNodeId == 0) return;

    runtime.nodeTimer += dt;

    for (const auto& transition : asset.transitions) {
        if (transition.fromNodeId != runtime.currentNodeId) continue;
        if (!IsInputPressed(transition.input, inputQueue)) continue;

        const GameLoopNode* toNode = asset.FindNode(transition.toNodeId);
        if (!toNode) return;

        runtime.pendingNodeId = toNode->id;
        runtime.pendingScenePath = toNode->scenePath;
        runtime.sceneTransitionRequested = true;
        runtime.forceReload = (toNode->id == runtime.currentNodeId);
        return;
    }
}
