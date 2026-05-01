#include "SceneTransitionSystem.h"

#include "Asset/PrefabSystem.h"
#include "Console/Logger.h"
#include "GameLoopRuntime.h"
#include "Registry/Registry.h"

namespace
{
    void ApplySuccessfulTransition(GameLoopRuntime& runtime)
    {
        runtime.previousNodeId = runtime.currentNodeId;
        runtime.currentNodeId = runtime.pendingNodeId;
        runtime.currentScenePath = runtime.pendingScenePath;
        runtime.pendingNodeId = 0;
        runtime.pendingScenePath.clear();
        runtime.sceneTransitionRequested = false;
        runtime.waitingSceneLoad = false;
        runtime.forceReload = false;
        runtime.nodeTimer = 0.0f;
        runtime.observedActorPositionInitialized = false;
    }

    void DiscardPendingTransition(GameLoopRuntime& runtime)
    {
        runtime.pendingNodeId = 0;
        runtime.pendingScenePath.clear();
        runtime.sceneTransitionRequested = false;
        runtime.waitingSceneLoad = false;
        runtime.forceReload = false;
    }
}

bool SceneTransitionSystem::UpdateEndOfFrame(
    GameLoopRuntime& runtime,
    Registry& gameRegistry,
    SceneFileMetadata* outMetadata)
{
    if (!runtime.sceneTransitionRequested) {
        return false;
    }

    if (runtime.pendingScenePath.empty()) {
        LOG_ERROR("[SceneTransitionSystem] pendingScenePath is empty while a transition was requested");
        DiscardPendingTransition(runtime);
        return false;
    }

    runtime.waitingSceneLoad = true;

    SceneFileMetadata metadata;
    const bool ok = PrefabSystem::LoadSceneIntoRegistry(
        runtime.pendingScenePath,
        gameRegistry,
        &metadata);

    if (!ok) {
        LOG_ERROR("[SceneTransitionSystem] LoadSceneIntoRegistry failed: %s", runtime.pendingScenePath.c_str());
        DiscardPendingTransition(runtime);
        return false;
    }

    if (outMetadata) {
        *outMetadata = metadata;
    }

    ApplySuccessfulTransition(runtime);
    return true;
}