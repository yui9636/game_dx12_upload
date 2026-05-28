#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "GameLoopAsset.h"

// GameLoop の実行時状態。シーン読み込みをまたいで保持する。
// EngineKernel が所有する（Registry では所有しない）。
struct GameLoopRuntime
{
    // グラフ内の現在ノード ID。
    uint32_t currentNodeId  = 0;

    // 直前ノード ID（デバッグ / 演出用に保持）。
    uint32_t previousNodeId = 0;

    // 遷移待ちの次ノード ID（SceneTransitionSystem が消費）。
    uint32_t pendingNodeId  = 0;

    std::string currentScenePath;
    std::string pendingScenePath;

    // Loading scene など、表示だけ差し替えて currentNodeId は進めない読み込みを区別する。
    bool pendingSceneAdvancesNode = true;

    // 遷移要求中なら true（GameLoopSystem が設定し、
    // SceneTransitionSystem がクリアする）。
    bool sceneTransitionRequested = false;

    // 同期ロード中なら true。
    // Phase 1 では短時間だけ使う。将来の非同期ロード用の置き場。
    bool waitingSceneLoad = false;

    // pendingScenePath == currentScenePath でも強制リロードする。
    bool forceReload = false;

    // 現在ノード開始からの時間（秒）。TimerElapsed で使う。
    float nodeTimer = 0.0f;

    // Durable FlowEvent 履歴を読む条件が、現在ノードに入る前のイベントを拾わないための境界。
    uint64_t nodeEventSequenceCursor = 0;

    // ActorMovedDistance: ノード開始時点の監視対象 actor 位置。
    DirectX::XMFLOAT3 observedActorStartPosition{ 0.0f, 0.0f, 0.0f };
    bool              observedActorPositionInitialized = false;

    // RuntimeFlag / CustomEvent のフラグテーブル。
    std::unordered_map<std::string, bool> flags;

    std::vector<QueuedGameFlowAction> pendingActions;
    float actionWaitRemaining = 0.0f;
    float fadeRemaining = 0.0f;
    float fadeDuration = 0.0f;
    float fadeAlpha = 0.0f;
    bool loadingOverlayVisible = false;
    std::string loadingMessage;

    // GameLoop 実行中（Play 中）なら true。
    bool isActive = false;

    // Stop 後の初期状態へ戻す。
    void Reset();
};
