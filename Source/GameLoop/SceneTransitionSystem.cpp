// SceneTransitionSystem の GameLoop 関連実装をまとめます。
#include "SceneTransitionSystem.h"

#include "Asset/PrefabSystem.h"
#include "Console/Logger.h"
#include "GameLoopRuntime.h"
#include "Registry/Registry.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "System/Query.h"
#include "Component/NameComponent.h"

namespace
{
    void ApplySuccessfulTransition(GameLoopRuntime& runtime)
    {
        runtime.currentScenePath = runtime.pendingScenePath;

        if (runtime.pendingSceneAdvancesNode) {
            runtime.previousNodeId = runtime.currentNodeId;
            runtime.currentNodeId = runtime.pendingNodeId;
            runtime.nodeTimer = 0.0f;
            runtime.observedActorPositionInitialized = false;
        }

        runtime.pendingNodeId = 0;
        runtime.pendingScenePath.clear();
        runtime.pendingSceneAdvancesNode = true;
        runtime.sceneTransitionRequested = false;
        runtime.waitingSceneLoad = false;
        runtime.forceReload = false;
    }

    void DiscardPendingTransition(GameLoopRuntime& runtime)
    {
        runtime.pendingNodeId = 0;
        runtime.pendingScenePath.clear();
        runtime.pendingSceneAdvancesNode = true;
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

    // Defensive check: gameflow auto-load 後に Player / Enemy entity が必須 component を
    // 持っているか log。ユーザー報告の「PlayerTag/HealthComponent が落ちる」現象の早期検出。
    {
        int playerCount = 0;
        int playerMissingHealth = 0;
        int playerMissingState = 0;
        Query<PlayerTagComponent, NameComponent> q(gameRegistry);
        q.ForEachWithEntity([&](EntityID e, PlayerTagComponent& tag, NameComponent& name) {
            (void)tag;
            ++playerCount;
            if (!gameRegistry.GetComponent<HealthComponent>(e)) {
                ++playerMissingHealth;
                LOG_WARN("[SceneTransition] Player entity %s ('%s') missing HealthComponent after scene load",
                    std::to_string(e).c_str(), name.name.c_str());
            }
        });
        if (playerCount == 0) {
            LOG_INFO("[SceneTransition] Scene loaded but no PlayerTag entity found (may be intentional for non-battle scenes).");
        } else if (playerMissingHealth > 0) {
            LOG_WARN("[SceneTransition] %d/%d player entities are missing required runtime components. Re-running EnsureAllPlayerRuntimeComponents...",
                playerMissingHealth, playerCount);
            // 念のため再実行 (EngineKernel.cpp 側でも呼んでいるが、idempotent なので二重呼び OK)
            PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents(gameRegistry, false);
        }
    }
    return true;
}
