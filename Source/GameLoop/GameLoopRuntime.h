#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include <DirectXMath.h>

// scene を跨いで保持される GameLoop の runtime 状態。
// Registry には載せず、EngineKernel が所有する。
struct GameLoopRuntime
{
    // graph 上の現在 node id。
    uint32_t currentNodeId  = 0;

    // 直前 node id。debug や演出で参照可能。
    uint32_t previousNodeId = 0;

    // 遷移要求中の next node id。SceneTransitionSystem が消化する。
    uint32_t pendingNodeId  = 0;

    std::string currentScenePath;
    std::string pendingScenePath;

    // 遷移要求が出ているか。GameLoopSystem が立て、SceneTransitionSystem がクリアする。
    bool sceneTransitionRequested = false;

    // 同期 load 中フラグ。Phase 1 では実質単フレーム扱いだが、async 化への布石。
    bool waitingSceneLoad = false;

    // current==pending でも強制 reload する場合に true。
    bool forceReload = false;

    // node 開始からの経過時間 (TimerElapsed condition 用)。
    float nodeTimer = 0.0f;

    // ActorMovedDistance 用、node 開始時に観測対象 actor の位置を控える。
    DirectX::XMFLOAT3 observedActorStartPosition{ 0.0f, 0.0f, 0.0f };
    bool              observedActorPositionInitialized = false;

    // RuntimeFlag / CustomEvent condition 用フラグテーブル。
    std::unordered_map<std::string, bool> flags;

    // GameLoop が進行中か (Play 中の有効フラグ)。
    bool isActive = false;

    // すべての runtime state を Editor 戻り時の初期状態にリセットする。
    void Reset();
};
