// GameLoopRuntime の GameLoop 関連実装をまとめます。
#include "GameLoopRuntime.h"

void GameLoopRuntime::Reset()
{
    currentNodeId  = 0;
    previousNodeId = 0;
    pendingNodeId  = 0;
    currentScenePath.clear();
    pendingScenePath.clear();
    pendingSceneAdvancesNode = true;
    sceneTransitionRequested = false;
    waitingSceneLoad         = false;
    forceReload              = false;
    nodeTimer                = 0.0f;
    observedActorStartPosition = { 0.0f, 0.0f, 0.0f };
    observedActorPositionInitialized = false;
    flags.clear();
    pendingActions.clear();
    actionWaitRemaining = 0.0f;
    fadeRemaining = 0.0f;
    fadeDuration = 0.0f;
    fadeAlpha = 0.0f;
    loadingOverlayVisible = false;
    loadingMessage.clear();
    isActive = false;
}
