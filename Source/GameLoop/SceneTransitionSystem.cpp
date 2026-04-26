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
        // flags は scene 跨ぎで保持する。
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
        LOG_ERROR("[SceneTransitionSystem] pendingScenePath が空のまま遷移要求が来ました");
        DiscardPendingTransition(runtime);
        return;
    }

    const bool sameScene = (runtime.pendingScenePath == runtime.currentScenePath);
    if (sameScene && !runtime.forceReload) {
        // node id だけ更新し、scene の reload は行わない。
        ApplySuccessfulTransition(runtime);
        return;
    }

    runtime.waitingSceneLoad = true;
    EngineKernel::Instance().ResetRenderStateForSceneChange();

    const bool ok = PrefabSystem::LoadSceneIntoRegistry(runtime.pendingScenePath, gameRegistry);
    if (!ok) {
        LOG_ERROR("[SceneTransitionSystem] LoadSceneIntoRegistry 失敗: %s", runtime.pendingScenePath.c_str());
        // currentNode / currentScenePath / nodeTimer / flags は保持。
        DiscardPendingTransition(runtime);
        return;
    }

    ApplySuccessfulTransition(runtime);
}
