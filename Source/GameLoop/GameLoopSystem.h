#pragma once

class Registry;
struct GameLoopAsset;
struct GameLoopRuntime;
class UIButtonClickEventQueue;

// 現在 node の transition condition を評価し、
// 成立した遷移があれば runtime.pending* と sceneTransitionRequested を立てる。
// 実 scene load は SceneTransitionSystem に委ねる。
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
