#pragma once

class Registry;
struct GameLoopAsset;
struct GameLoopRuntime;
class UIButtonClickEventQueue;

// Evaluate transition conditions for the current node.
// On a satisfied transition, set runtime.pending* and sceneTransitionRequested.
// Does NOT load scenes (SceneTransitionSystem does that).
class GameLoopSystem
{
public:
    static void Update(
        const GameLoopAsset&           asset,
        GameLoopRuntime&               runtime,
        Registry&                      gameRegistry,
        Registry&                      gameLoopRegistry,
        const UIButtonClickEventQueue& clickQueue,
        float                          dt);
};
