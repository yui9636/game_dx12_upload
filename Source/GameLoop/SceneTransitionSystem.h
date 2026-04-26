#pragma once

class Registry;
struct GameLoopRuntime;

// frame 末尾 (GameLayer::Update 終了後 / Render 開始前) で
// pendingScenePath を実 scene load に消化する。
// condition 評価は行わない。
class SceneTransitionSystem
{
public:
    static void UpdateEndOfFrame(
        GameLoopRuntime& runtime,
        Registry&        gameRegistry);
};
