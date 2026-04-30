#pragma once

class Registry;
class UIButtonClickEventQueue;
class InputEventQueue;
struct GameLoopAsset;
struct GameLoopRuntime;

class GameLoopSystem
{
public:
    static void Update(
        const GameLoopAsset&           asset,
        GameLoopRuntime&               runtime,
        Registry&                      gameRegistry,
        Registry&                      gameLoopRegistry,
        const UIButtonClickEventQueue& clickQueue,
        const InputEventQueue&         inputQueue,
        float                          dt);
};
