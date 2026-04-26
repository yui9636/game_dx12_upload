#include "GameLoopRuntime.h"

void GameLoopRuntime::Reset()
{
    currentNodeId  = 0;
    previousNodeId = 0;
    pendingNodeId  = 0;
    currentScenePath.clear();
    pendingScenePath.clear();
    sceneTransitionRequested = false;
    waitingSceneLoad         = false;
    forceReload              = false;
    nodeTimer                = 0.0f;
    observedActorStartPosition = { 0.0f, 0.0f, 0.0f };
    observedActorPositionInitialized = false;
    flags.clear();
    isActive = false;
}
