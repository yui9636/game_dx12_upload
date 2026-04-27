#include "SceneTransitionSystem.h"

#include "Asset/PrefabSystem.h"
#include "Engine/EngineKernel.h"
#include "GameLoopRuntime.h"
#include "Registry/Registry.h"
#include "Console/Logger.h"

namespace
{
    void ApplySuccessfulTransition(GameLoopRuntime& runtime)
    {
        runtime.previousNodeId             = runtime.currentNodeId;
        runtime.currentNodeId              = runtime.pendingNodeId;
        runtime.currentScenePath           = runtime.pendingScenePath;
        runtime.pendingNodeId              = 0;
        runtime.pendingScenePath.clear();
        runtime.sceneTransitionRequested   = false;
        runtime.waitingSceneLoad           = false;
        runtime.forceReload                = false;
        runtime.nodeTimer                  = 0.0f;
        runtime.observedActorPositionInitialized = false;
        // flags are kept across scenes.
    }

    void DiscardPendingTransition(GameLoopRuntime& runtime)
    {
        runtime.pendingNodeId              = 0;
        runtime.pendingScenePath.clear();
        runtime.sceneTransitionRequested   = false;
        runtime.waitingSceneLoad           = false;
        runtime.forceReload                = false;
    }
}

void SceneTransitionSystem::UpdateEndOfFrame(GameLoopRuntime& runtime, Registry& gameRegistry)
{
    if (!runtime.sceneTransitionRequested) return;

    if (runtime.pendingScenePath.empty()) {
        LOG_ERROR("[SceneTransitionSystem] pendingScenePath is empty while a transition was requested");
        DiscardPendingTransition(runtime);
        return;
    }

    const bool sameScene = (runtime.pendingScenePath == runtime.currentScenePath);
    if (sameScene && !runtime.forceReload) {
        // Update node id but skip the actual scene reload.
        ApplySuccessfulTransition(runtime);
        return;
    }

    runtime.waitingSceneLoad = true;
    EngineKernel::Instance().ResetRenderStateForSceneChange();

    const bool ok = PrefabSystem::LoadSceneIntoRegistry(runtime.pendingScenePath, gameRegistry);
    if (!ok) {
        LOG_ERROR("[SceneTransitionSystem] LoadSceneIntoRegistry failed: %s", runtime.pendingScenePath.c_str());
        // Keep current scene state.
        DiscardPendingTransition(runtime);
        return;
    }

    ApplySuccessfulTransition(runtime);
}
