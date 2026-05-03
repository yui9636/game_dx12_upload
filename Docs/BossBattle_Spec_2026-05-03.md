# 1v1 ボス戦 完成仕様書 v1.0

作成日: 2026-05-03
関連:
- `Docs/EnemyAI_BehaviorTree_Spec_v1.0_2026-04-27.md`
- `Docs/ActorEditor_StateBoundBT_Spec_v2.0_2026-04-27.md`
- `Docs/PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md`
- `Docs/PlayerEditor_StateMachine_Ownership_Spec_2026-04-25.md`

## 0. 目的

既存の Locomotion / StateMachine / Timeline / BehaviorTree / Collision / Health の各レイヤを **「Body × Attack の衝突 → ダメージ → 被弾リアクション → HUD 反映 → 勝敗」** の 1 本のパイプラインに連結し、1 対 1 のボス戦を成立させる。**新規アクターやアセット形式は導入しない**。既存の `EnemyEditorSetupFullEnemy` / `EnsurePlayerRuntimeComponents` / Timeline (Hitbox/Shake) / `CameraTPVControlComponent` / `UIProgressBar2D` / `DamageTextManager` を最大限活かす。

## 1. 範囲

### 1.1 やること
- Body × Attack コリジョンを HP 減算に変換する `DamageSystem`
- ヒット側の HUD 駆動 (`HUDBindingSystem`)
- 勝敗フロー (`BattleFlowSystem`)
- ロックオン (`LockOnSystem`) と Inspector の Entity ピッカー
- `ThirdPersonCameraSystem` の GameLayer 登録 + `TimelineShakeSystem` のオフセット適用
- `EnsurePlayerRuntimeComponents` / `EnsureEnemyRuntimeComponents` への新規コンポーネント追加（既存ワークフローを壊さない）

### 1.2 やらないこと
- 新規ヒットボックス形状・新規 Timeline item type の追加
- 新規アニメシステム / 新規 BT ノード（既存 BT で `Health` 条件評価は可能 → ボスフェーズも既存機能で OK）
- マルチプレイ・複数敵協調・NavMesh

### 1.3 既存の何を変更しないか
- `TimelineHitboxSystem` / `TimelineShakeSystem` / `PlaybackSystem` の挙動
- `HitStopComponent` 仕様（**Timeline の Shake item から自動セット済 → そのまま使う**）
- `StateMachineSystem` の `Damaged` トリガ評価
- `CollisionManager.ComputeAllContacts()`

## 2. 既存インフラ確定事項

### 2.1 Body × Attack の衝突がダメージのトリガ ── 確認済

**結論: その認識で正しい**。実フロー:

```
Animator (frame 進行)
  ↓
TimelineHitboxSystem  : Timeline item type=0 (Hitbox) を読み、攻撃中のみ
                        ColliderAttribute::Attack の Sphere を CollisionManager に動的登録
                        userPtr = 攻撃側 EntityID
  ↓
CollisionSystem        : 全 ColliderComponent (Body) の位置を CollisionManager に同期
  ↓
CollisionManager
  .ComputeAllContacts(): 全ペアの contacts 一覧を生成（attribute は問わない）
  ↓
DamageSystem ★新規   : contacts のうち (Attack vs Body) のみ拾う
                        ・自陣ペアを除外 (Team 判定)
                        ・既ヒット相手を除外 (HitboxTrackingComponent)
                        ・無敵 / 死亡を除外
                        → DamageEventComponent.events に push
  ↓
HealthSystem 拡張     : events を消費して HP を減算、Damaged トリガを叩く、
                        DamageTextManager / Effect / Audio を発行
                        被弾側にも HitStop を伝播
```

判定属性は [Collision.h:15](../Source/Collision/Collision.h:15) の `ColliderAttribute { Body=0, Attack=1 }` 2 値だけで充足する。

### 2.2 HitStop は Timeline から既に駆動されている ── 認識訂正

| 場所 | 役割 |
|---|---|
| [TimelineShakeSystem.cpp:42-49](../Source/Gameplay/TimelineShakeSystem.cpp:42) | Timeline item type=4 (Shake) に入る瞬間、`HitStopComponent.timer = item.shake.hitStopDuration` / `speedScale = item.shake.timeScale` を書き込む |
| [PlaybackSystem.cpp:55-62](../Source/Gameplay/PlaybackSystem.cpp:55) | `HitStopComponent.timer > 0` の間は `timeScale` をオーバーライドし、毎フレーム timer を減算 |

→ **新規に `HitStopSystem` を追加してはいけない**。攻撃側のヒットストップはアニメ作者が PlayerEditor の Timeline 上で Shake item を置けば既に成立する。被弾側だけ別経路で `HitStopComponent.timer` を書き込む必要があるが、これは `HealthSystem` 拡張側で 1 行で済む。

### 2.3 PlayerEditor のアクター切替ワークフロー ── 確認済

| ActorEditorMode | ボタン | 中身 |
|---|---|---|
| `Player` | Setup Full Player | `PlayerRuntimeSetup::EnsurePlayerRuntimeComponents` |
| `Enemy` | Setup Full Enemy | `EnemyEditorSetupFullEnemy` → `EnemyRuntimeSetup::EnsureEnemyRuntimeComponents` ([EnemyRuntimeSetup.cpp:46](../Source/AI/EnemyRuntimeSetup.cpp:46)) |
| `NPC` | Setup Full NPC | `EnemyRuntimeSetup::EnsureNPCRuntimeComponents` |

**新しい必須コンポーネントは必ず上記 3 関数に追記する**。Inspector で手動付与する設計にしない（漏れる）。

### 2.4 ThirdPersonCameraSystem ── **GameLayer に登録されていない**

[ThirdPersonCameraSystem.cpp](../Source/Camera/ThirdPersonCameraSystem.cpp) は実装済だが、[GameLayer.cpp:160](../Source/Layer/GameLayer.cpp:160) では `FreeCameraSystem::Update` のみが呼ばれている。さらに 2 件の問題がある:

1. `TimelineShakeSystem::GetShakeOffset()` を読んでいない → Shake item を置いてもカメラが揺れない
2. `ImGui::GetIO().MouseDown[Right]` を直接読む → ゲーム実行中のみ Yaw/Pitch を入力したいケースに、エディタ操作と混線する可能性。最低限 EngineMode ガードが要る

→ §6.5 で対処。

## 3. 新規コンポーネント

すべて GEHeaderTool の対象としてリフレクション登録する（[ComponentMeta.generated.h](../Source/Generated/ComponentMeta.generated.h) の自動再生成）。

### 3.1 `TeamComponent`

```cpp
// Source/Gameplay/TeamComponent.h
#pragma once
#include <cstdint>
struct TeamComponent {
    uint8_t teamId = 0;   // 0=Player陣営, 1=Enemy陣営。同値はフレンドリーファイア扱いで除外
};
```

### 3.2 `DamageEventComponent`（フレーム単位イベントキュー）

```cpp
// Source/Gameplay/DamageEventComponent.h
#pragma once
#include <vector>
#include <DirectXMath.h>
#include "Entity/Entity.h"

struct DamageEventComponent {
    struct Event {
        EntityID attacker = Entity::NULL_ID;
        EntityID victim   = Entity::NULL_ID;
        int      amount   = 0;
        DirectX::XMFLOAT3 hitPoint     { 0,0,0 };
        DirectX::XMFLOAT3 knockbackDir { 0,0,0 };
        float    knockbackPower = 0.0f;
        float    hitStopSec     = 0.08f;   // 被弾側に伝播する HitStop
        uint8_t  reactionKind   = 0;       // 0=light, 1=heavy, 2=launch（v1 は 0 のみ）
    };
    std::vector<Event> events;             // 同フレーム末で clear
};
```

シングルトン entity に 1 個付ける運用。`GameLayer::Initialize` で `DamageEventEntity` を生成する。

### 3.3 `HUDLinkComponent`（HP UI と HealthComponent の紐付け）

```cpp
// Source/Gameplay/HUDLinkComponent.h
#pragma once
#include <cstdint>
struct HUDLinkComponent {
    bool asPlayerHUD  = false;   // 画面下プレイヤー HP バーに反映
    bool asBossHUD    = false;   // 画面上ボス HP バーに反映
    bool asWorldFloat = true;    // 頭上 3D HP バー (UIProgressBar3D / UIHPNumber)
    float worldOffsetY = 2.0f;   // 頭上ゲージの Y オフセット
};
```

### 3.4 `LockOnTargetComponent`（プレイヤー側）

```cpp
// Source/Gameplay/LockOnTargetComponent.h
#pragma once
#include "Entity/Entity.h"
struct LockOnTargetComponent {
    EntityID currentTarget = Entity::NULL_ID;
    float maxRange = 25.0f;
    float fovRadians = 1.5708f;
    bool sticky = true;          // 視界外でも一定時間保持
};
```

### 3.5 `BattleFlowComponent`（シングルトン entity）

```cpp
// Source/GameLoop/BattleFlowComponent.h
#pragma once
#include <cstdint>
#include "Entity/Entity.h"
struct BattleFlowComponent {
    enum class Phase : uint8_t { Idle=0, Encounter=1, Combat=2, Victory=3, Defeat=4 };
    Phase    phase            = Phase::Idle;
    float    phaseTimer       = 0.0f;
    EntityID playerEntity     = Entity::NULL_ID;
    EntityID bossEntity       = Entity::NULL_ID;
    EntityID arenaEntity      = Entity::NULL_ID;     // StageBoundsComponent を持つ
    float    encounterRadius  = 18.0f;
};
```

## 4. 新規システム

### 4.1 `DamageSystem`（最重要 / P0）

`Source/Gameplay/DamageSystem.{h,cpp}`

```
入力: CollisionManager::Instance().ComputeAllContacts(out)
出力: シングルトン entity の DamageEventComponent.events

毎フレーム処理:
  for each contact in out:
    A = CollisionManager.Get(contact.idA)
    B = CollisionManager.Get(contact.idB)
    if (A.attribute == Attack && B.attribute == Body) → attackerCol=A, victimCol=B
    else if (A.attribute == Body && B.attribute == Attack) → attackerCol=B, victimCol=A
    else continue

    attackerEntity = (EntityID)(uintptr_t)attackerCol.userPtr
    victimEntity   = (EntityID)(uintptr_t)victimCol.userPtr
    if (attackerEntity == victimEntity) continue        // セルフヒット除外
    if (両者の TeamComponent.teamId が等しい) continue   // フレンドリーファイア除外

    HitboxTracking* track = registry.GetComponent<HitboxTrackingComponent>(attackerEntity)
    if (track && track->hitEntities[]に victim 既存) continue
    HealthComponent* vh = registry.GetComponent<HealthComponent>(victimEntity)
    if (!vh || vh->isDead || vh->isInvincible) continue

    // ダメージ値解決
    int dmg = 1
    ActionStateComponent*    as = registry.GetComponent<ActionStateComponent>(attackerEntity)
    ActionDatabaseComponent* ad = registry.GetComponent<ActionDatabaseComponent>(attackerEntity)
    if (as && ad && 0 <= as->currentNodeIndex < ad->nodeCount):
        dmg = ad->nodes[as->currentNodeIndex].damageVal

    // ノックバック方向 = attacker → victim 水平
    DamageEventComponent::Event ev;
    ev.attacker = attackerEntity;
    ev.victim   = victimEntity;
    ev.amount   = dmg;
    ev.hitPoint = contact.hit.hitPosition;
    ev.knockbackDir = normalize(victimPos - attackerPos, y=0)
    ev.knockbackPower = 0.0f                 // v1 は未使用
    ev.hitStopSec = 0.08f
    eventQueue.events.push_back(ev)

    // 多段ヒット防止
    if (track) track->hitEntities[track->hitEntityCount++] = victimEntity
```

GameLayer 登録順: `CollisionSystem::Update` → `TimelineHitboxSystem::Update` → **`DamageSystem::Update`** → `HealthSystem::Update`。

### 4.2 `HealthSystem` 拡張

[HealthSystem.cpp](../Source/Gameplay/HealthSystem.cpp) を以下に拡張。

```
1. 既存: 全エンティティの invincibleTimer 減算 / isDead 判定
2. 追加: シングルトン entity の DamageEventComponent.events を消化
   for each ev in queue.events:
     vh = registry.GetComponent<HealthComponent>(ev.victim)
     vh->health -= ev.amount
     vh->lastDamage = ev.amount
     vh->invincibleTimer = 0.5f
     vh->isInvincible = true

     // ステートマシンへ Damaged トリガを送る
     auto* smp = registry.GetComponent<StateMachineParamsComponent>(ev.victim)
     if (smp) smp->SetParam("Damaged", 1.0f)

     // 被弾側にも HitStop を伝播（Timeline 経由でなく直接）
     auto* hs = registry.GetComponent<HitStopComponent>(ev.victim)
     if (hs && ev.hitStopSec > hs->timer) {
         hs->timer = ev.hitStopSec
         hs->speedScale = 0.05f
     }

     // 演出: ダメージテキスト・ヒットエフェクト・ヒット SE
     DamageTextManager::Instance().Spawn(ev.hitPoint, ev.amount)
     // EffectSpawnRequestComponent を一時 entity に積む（既存パターン踏襲）
     // AudioOneShotRequestComponent を一時 entity に積む

     // ノックバック適用
     auto* phys = registry.GetComponent<CharacterPhysicsComponent>(ev.victim)
     if (phys && ev.knockbackPower > 0.0f) {
         phys->velocity.x = ev.knockbackDir.x * ev.knockbackPower
         phys->velocity.z = ev.knockbackDir.z * ev.knockbackPower
     }

3. 末尾で events.clear()
```

### 4.3 `HUDBindingSystem`

`Source/UI/HUDBindingSystem.{h,cpp}`

```
Query<HealthComponent, HUDLinkComponent>:
  ratio = (float)health.health / max(1, health.maxHealth)
  if (link.asPlayerHUD)  s_playerBar2D->SetProgress(ratio)
  if (link.asBossHUD)    s_bossBar2D  ->SetProgress(ratio)
  if (link.asWorldFloat) 頭上 3D ゲージ更新（UIProgressBar3D::SetProgress, UIHPNumber::SetHP）

s_playerBar2D / s_bossBar2D は HUDBindingSystem 内 static インスタンス（プロトタイプ; 後で UIWorld/UIScreen entity 化）。
```

GameLayer 登録順: 描画前。`HealthSystem::Update` の後、`Render` 直前。

### 4.4 `LockOnSystem`

`Source/Gameplay/LockOnSystem.{h,cpp}`

```
1. PlayerTagComponent + LockOnTargetComponent を持つ entity を走査
2. 入力 ResolvedInputState の "LockOn" アクション rising-edge を検知
3. rising-edge:
     if (currentTarget == NULL):
        最寄りの (EnemyTagComponent, 距離 ≤ maxRange, fov 内, Raycast 遮蔽なし) を選定
        lockOn.currentTarget = found
        cameraTPV.target = found     // CameraTPVControlComponent を Player カメラから検索して書き換え
     else:
        lockOn.currentTarget = NULL
        cameraTPV.target = playerEntity   // 通常追従に戻す
4. ロックオン中はプレイヤーの LocomotionState.targetAngleY を target 方向へ補正（攻撃時のみ強くする magnetism は ActionDatabase 既存値で OK）
```

### 4.5 `BattleFlowSystem`

`Source/GameLoop/BattleFlowSystem.{h,cpp}`

```
state machine over BattleFlowComponent.phase:

Idle:
  プレイヤーがアリーナに侵入したら Encounter へ
Encounter:
  ボスをスポーン（または既存ボスを起動）
  CameraTPVControlComponent.target = boss にしてイントロカット
  phaseTimer ≥ introDuration → Combat
Combat:
  毎フレーム両者の Health を監視
  bossHealth.isDead → Victory
  playerHealth.isDead → Defeat
Victory / Defeat:
  Cinematic / リザルト UI トリガ
  入力で Reset
Reset:
  Health を満タンに戻し、Idle へ
```

GameLayer 登録順: 全 Gameplay system の最後。

## 5. 既存ファイルへの追記

### 5.1 `EnemyRuntimeSetup::EnsureEnemyRuntimeComponents` ([EnemyRuntimeSetup.cpp:46](../Source/AI/EnemyRuntimeSetup.cpp:46))

追加する `EnsureComponent<...>(...)` 行:

| 追加コンポーネント | 既定値 |
|---|---|
| `TeamComponent` | `teamId = 1` |
| `HUDLinkComponent` | `asBossHUD=true, asWorldFloat=false`（雑魚なら `asWorldFloat=true, asBossHUD=false`） |
| 既存 `ColliderComponent` に Body 要素 1 件をプリセット | `radius=0.5, attribute=Body` |

### 5.2 `PlayerRuntimeSetup::EnsurePlayerRuntimeComponents` ([PlayerRuntimeSetup.cpp](../Source/Gameplay/PlayerRuntimeSetup.cpp))

| 追加コンポーネント | 既定値 |
|---|---|
| `TeamComponent` | `teamId = 0` |
| `HUDLinkComponent` | `asPlayerHUD=true` |
| `LockOnTargetComponent` | デフォルト |
| `CameraTPVControlComponent`（プレイヤーカメラ entity 側）| `target = player entity` |

### 5.3 `GameLayer::Initialize` 追加

```cpp
// Damage event queue (singleton)
EntityID damageEventEntity = m_registry.CreateEntity();
m_registry.AddComponent(damageEventEntity, NameComponent{ "_DamageEventQueue" });
m_registry.AddComponent(damageEventEntity, DamageEventComponent{});

// Battle flow (singleton)
EntityID battleFlowEntity = m_registry.CreateEntity();
m_registry.AddComponent(battleFlowEntity, NameComponent{ "_BattleFlow" });
m_registry.AddComponent(battleFlowEntity, BattleFlowComponent{});
```

### 5.4 `GameLayer::Update` 追加 ([GameLayer.cpp:130〜180](../Source/Layer/GameLayer.cpp:130))

```cpp
// 既存
StateMachineSystem::Update(...)
LocomotionSystem::Update(...)
StaminaSystem::Update(...)
DodgeSystem::Update(...)
CharacterPhysicsSystem::Update(...)
TimelineSystem::Update(...)
ActionSystem::Update(...)
...
TimelineHitboxSystem::Update(...)        // (既存) Attack コライダー生成

// ★追加: ThirdPerson カメラ（player 用）
ThirdPersonCameraSystem::Update(m_registry, time.dt);

// ★追加: ロックオン（カメラより前に走らせて target を確定）
LockOnSystem::Update(m_registry, time.dt);
                                          // CollisionSystem::Update は既存システム順に従う
DamageSystem::Update(m_registry);         // ★追加: Body × Attack → events
HealthSystem::Update(m_registry, time.dt) // 拡張: events を消費

HUDBindingSystem::Update(m_registry);     // ★追加
BattleFlowSystem::Update(m_registry, time.dt); // ★追加: 最後に勝敗判定
```

### 5.5 `ThirdPersonCameraSystem` の手当 ([ThirdPersonCameraSystem.cpp](../Source/Camera/ThirdPersonCameraSystem.cpp))

1. **GameLayer に登録**（§5.4 で対応済）
2. EngineMode が Game でないなら `return`（エディタ操作と混線しないため）
3. `XMStoreFloat3(&trans.localPosition, currentPos)` の直後に `TimelineShakeSystem::GetShakeOffset()` を加算

```cpp
// 追加: ヒット時の画面揺れをカメラに反映
DirectX::XMFLOAT3 shake = TimelineShakeSystem::GetShakeOffset();
trans.localPosition.x += shake.x;
trans.localPosition.y += shake.y;
trans.localPosition.z += shake.z;
```

### 5.6 Inspector EntityID ピッカー ([InspectorECSUI.cpp:1745](../Source/Inspector/InspectorECSUI.cpp:1745))

現状は `ImGui::Text("%llu", value)` のみ。**「Inspector で enemy を指定」を成立させるための置換**:

```cpp
else if constexpr (std::is_same_v<T, EntityID>) {
    const char* label = "<None>";
    if (!Entity::IsNull(value)) {
        if (auto* name = registry->GetComponent<NameComponent>(value)) {
            label = name->name.c_str();
        } else {
            label = "<Unnamed>";
        }
    }
    if (ImGui::Button(label, ImVec2(-1, 0))) ImGui::OpenPopup("EntityPicker");
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* p = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            value = *static_cast<const EntityID*>(p->Data);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopup("EntityPicker")) {
        if (ImGui::Selectable("<None>")) { value = Entity::NULL_ID; changed = true; }
        Query<NameComponent> q(*registry);
        q.ForEachWithEntity([&](EntityID e, NameComponent& n) {
            ImGui::PushID(static_cast<int>(e));
            if (ImGui::Selectable(n.name.c_str())) { value = e; changed = true; }
            ImGui::PopID();
        });
        ImGui::EndPopup();
    }
}
```

`HierarchyECSUI.cpp` 側で `ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityID))` を発行する処理も追加。

## 6. ボスエンティティのオーサリング手順（完成後の使い方）

1. PlayerEditor 起動 → ActorEditorMode を **Enemy** に切替
2. モデル読込 → `Setup Full Enemy` ボタン押下
   → `EnsureEnemyRuntimeComponents` が `EnemyTag` / `Health` / `BehaviorTree` / `StateMachine` / **`TeamComponent(1)`** / **`HUDLinkComponent(asBossHUD=true)`** などを一括追加
3. Skeleton パネルで Body コライダーを追加
4. Animation を選択 → Timeline パネルで攻撃アニメに **Hitbox item (type=0)** と **Shake item (type=4 / hitStopDuration を設定)** を打つ
5. StateMachine パネルで `Damaged`（既存）/`Dead` 遷移を確認
6. シーンに配置 → Inspector で `HealthComponent.maxHealth = 2000` などボス用に上書き
7. プレイヤー側も同様に `Setup Full Player`、`HUDLinkComponent.asPlayerHUD=true`、`TeamComponent(0)`
8. シーンに `_BattleFlow` entity の `BattleFlowComponent.bossEntity` / `playerEntity` / `arenaEntity` を **Inspector の Entity ピッカー** から指定

## 7. 実装順 (推奨)

| 順 | タスク | 所要 | 依存 |
|---|---|---|---|
| 1 | `TeamComponent` / `DamageEventComponent` / `HUDLinkComponent` / `LockOnTargetComponent` / `BattleFlowComponent` 追加 → GEHeaderTool 再生成 | 半日 | なし |
| 2 | `EnsurePlayerRuntimeComponents` / `EnsureEnemyRuntimeComponents` に追記 | 半日 | 1 |
| 3 | `DamageSystem` 実装 + GameLayer 登録 + Damage event entity 生成 | 1 日 | 1, 2 |
| 4 | `HealthSystem` 拡張（events 消費 / 演出発火 / 被弾 HitStop 伝播） | 半日 | 3 |
| 5 | `ThirdPersonCameraSystem` を GameLayer に登録 + Shake オフセット適用 + EngineMode ガード | 半日 | なし |
| 6 | `HUDBindingSystem` + 画面 HP バー描画 | 1 日 | 4 |
| 7 | Inspector EntityID ピッカー + Hierarchy DragDrop payload | 半日 | なし |
| 8 | `LockOnSystem` | 1 日 | 5, 7 |
| 9 | `BattleFlowSystem` + 簡易 Result UI | 1〜2 日 | 4, 6 |
| 10 | ボス用 BT (フェーズ条件 `Health < 0.5*MaxHealth` 等) を BehaviorTreeEditor で構築 | 任意 | 9 |

**山は #3 の DamageSystem**。これが入った瞬間に「殴れば HP が減る」が成立し、残りは仕上げ。

## 8. 受入条件 (Validation)

- [ ] プレイヤーが攻撃モーション中、Body コライダーを持つボスに当てた時のみ HP が減る
- [ ] 同じスイングで同一相手から複数回ダメージを受けない
- [ ] 自分の Attack が自分の Body に当たっても無効
- [ ] プレイヤー同士・敵同士でフレンドリーファイアが起きない（TeamComponent）
- [ ] 無敵時間 0.5s が機能し、点滅させずとも連続ヒットが起きない
- [ ] HP=0 で `isDead=true` → StateMachine が Dead 遷移
- [ ] Timeline の Shake item で攻撃側のヒットストップが効く
- [ ] DamageSystem 経由で被弾側にもヒットストップが効く
- [ ] ヒット位置にダメージ数値が浮き上がる
- [ ] 画面下バー（プレイヤー）/ 画面上バー（ボス）/ 頭上 3D ゲージのいずれも `HUDLinkComponent` の boolean で切替可能
- [ ] L スティック押し込み等で最寄りの Enemy にロックオンし、`CameraTPVControlComponent.target` が切り替わる
- [ ] Timeline Shake で実際にゲーム画面が揺れる（ThirdPersonCameraSystem 適用後）
- [ ] ボス HP=0 で BattleFlow が Victory に遷移
- [ ] `Inspector` で `BattleFlowComponent.bossEntity` 等のフィールドにエンティティ名がボタン表示され、クリックでピッカーが開く

## 9. リスク・留意

| 項目 | リスク | 対処 |
|---|---|---|
| `HitboxTrackingComponent.hitEntities[16]` の固定長 | 16 を超える多段ヒットで漏れる | v1 は 1v1 なので 16 で十分。将来拡張時に動的化 |
| `DamageEventComponent.events` の生存期間 | フレーム末で clear する責任分担が曖昧だと二重消費 | `HealthSystem::Update` の末尾で `events.clear()` と明文化（§4.2 末尾） |
| `BattleFlowSystem` がプレイ開始時 `playerEntity` 未指定だとクラッシュ | NULL チェック必須 / Inspector 警告 | システム冒頭で `IsNull` ガード |
| ThirdPersonCameraSystem の右クリックドラッグが Editor の Viewport 操作と競合 | EngineMode==Game ガードで切り分け |
