#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include <DirectXMath.h>

// Runtime state for the GameLoop. Persistent across scene loads.
// Owned by EngineKernel (NOT by any Registry).
struct GameLoopRuntime
{
    // Current node id in the graph.
    uint32_t currentNodeId  = 0;

    // Previous node id (kept for debug/effects).
    uint32_t previousNodeId = 0;

    // Pending next node id (consumed by SceneTransitionSystem).
    uint32_t pendingNodeId  = 0;

    std::string currentScenePath;
    std::string pendingScenePath;

    // True while a transition is requested (set by GameLoopSystem,
    // cleared by SceneTransitionSystem).
    bool sceneTransitionRequested = false;

    // True while a synchronous load is in progress.
    // Phase 1 keeps this short-lived; placeholder for future async loads.
    bool waitingSceneLoad = false;

    // Force a reload even if pendingScenePath == currentScenePath.
    bool forceReload = false;

    // Time (seconds) since the current node started. Used by TimerElapsed.
    float nodeTimer = 0.0f;

    // ActorMovedDistance: position of the observed actor when the node started.
    DirectX::XMFLOAT3 observedActorStartPosition{ 0.0f, 0.0f, 0.0f };
    bool              observedActorPositionInitialized = false;

    // RuntimeFlag / CustomEvent flag table.
    std::unordered_map<std::string, bool> flags;

    // True while GameLoop is running (Play in progress).
    bool isActive = false;

    // Reset to the post-Stop initial state.
    void Reset();
};
