#pragma once

// 敵 Entity のランタイム構築・修復用ヘルパー関数を宣言するファイル。

#include <DirectXMath.h>

#include "Entity/Entity.h"

class Registry;
// 敵 1 種類分のアセット参照と基本ステータスをまとめた設定アセット。
struct EnemyConfigAsset;

struct StateMachineAsset;

// 敵ランタイム構築用の関数群をまとめる名前空間。
namespace EnemyRuntimeSetup
{
    // EnemyTag を持つ全 Entity に敵ランタイム構成を適用する。
    void EnsureAllEnemyRuntimeComponents(Registry& registry, bool resetRuntimeState);

    // 敵として動作するために必要な全ランタイムコンポーネントを揃える。
    void EnsureEnemyRuntimeComponents(Registry& registry, EntityID entity);

    // NPC として動作するための最小ランタイム構成を揃える。
    void EnsureNPCRuntimeComponents(Registry& registry, EntityID entity);

    // 敵 AI の一時状態・ヘイト・移動入力を初期化する。
    void ResetEnemyRuntimeState(Registry& registry, EntityID entity);

    // EnemyConfigAsset の内容から敵 Entity を生成して初期化する。
    EntityID SpawnFromConfig(Registry& registry,
                             const EnemyConfigAsset& config,
                             const DirectX::XMFLOAT3& position);
}

// エディタ用に Enemy の標準構成と StateMachine を作成する。
void EnemyEditorSetupFullEnemy(Registry& registry, EntityID entity, StateMachineAsset& sm);
// エディタ操作から NPC Entity をフルセットアップする。
void EnemyEditorSetupFullNPC  (Registry& registry, EntityID entity, StateMachineAsset& sm);
// エディタ操作から敵ランタイムコンポーネントの不足を修復する。
void EnemyEditorRepairRuntime (Registry& registry, EntityID entity);
