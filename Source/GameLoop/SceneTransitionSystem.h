#pragma once

class Registry;
struct GameLoopRuntime;

// At end-of-frame (after GameLayer::Update, before EngineKernel::Render),
// consume the pending transition by loading the target scene.
// Does NOT evaluate conditions.
class SceneTransitionSystem
{
public:
    static void UpdateEndOfFrame(
        GameLoopRuntime& runtime,
        Registry&        gameRegistry);
};
