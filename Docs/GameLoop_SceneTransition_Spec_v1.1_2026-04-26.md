# GameLoop Scene Transition 仕様書 v1.1

作成日: 2026-04-26
改訂日: 2026-04-26
基底版: `GameLoop_SceneTransition_Spec_2026-04-26.md`

## 0. 改訂方針 (v1.0 → v1.1)

v1.0 は責務分離の概念を確立したが、実装着手時に曖昧となる箇所が複数存在した。v1.1 では次の方針で全項目を**確定文**化する。

- **「候補」「想定」を排除し、すべて確定仕様にする**
- **2D UI Button による scene 遷移**を一級の condition として組み込む
- **Play 開始時から Clear 到達までの完全フロー**を定義する
- **EngineKernel との結合点**を厳格に定義する
- **エラー / Stop / Pause / 同一 scene 再 load** の挙動を全件明示する
- **Frame phase 順** (Update / EndOfFrame / Render) を厳密に定義する

v1.0 と v1.1 で衝突する記述があった場合、**v1.1 を採用する**。

## 1. 目的

ユーザーが Editor 上で **Play ボタン** を押した瞬間に Title scene が表示され、Title 上の **2D Start ボタン** を押すと Battle scene に遷移、Battle scene の **Clear 条件** を満たすと Result scene に遷移、Retry / Cancel で再戦・タイトル復帰ができる、という一連の **gameplay 進行** を、scene 単位で組み立てて authoring / serialize / runtime 実行できる仕組みを定義する。

GameLoop は次のものを **扱う**。

- scene 単位の進行 graph (node + transition)
- transition 成立条件 (input action / 2D UI ボタン / actor 状態 / 経過時間 / runtime flag)
- scene をまたいで残る進行状態 (`GameLoopRuntime`)
- Play / Stop / Pause との連動
- editor authoring panel

GameLoop は次のものを **扱わない**。

- Player の attack / dodge / combo 等の gameplay runtime pipeline
- scene save / load 機構そのものの再設計
- prefab 機構そのものの再設計
- 戦闘勝敗ロジックの hard-coded 実装

## 2. 標準フロー (v1.1 受け入れ目標)

```text
[Editor]
  ↓ Play ボタン押下 (EngineKernel::Play())
[GameLoop 起動]
  ↓ asset.startNodeId = Title を currentNode にセット
  ↓ SceneTransitionSystem が Title.scene を load
[Title scene]
  - 2D UI: "Start" ボタン (Sprite + RectTransform + UIButtonComponent)
  - 2D UI: "Quit" ボタン (任意)
  ↓ Start ボタン click または InputPressed Confirm
[Battle scene]
  - 通常 gameplay
  ↓ Clear 条件 (例: AllActorsDead Enemy / ActorMovedDistance Player 1.0)
[Result scene]
  - 2D UI: "Retry" ボタン
  - 2D UI: "Title" ボタン
  ↓ Retry click or InputPressed Retry
[Battle scene] (reload)

  ↓ Title click or InputPressed Cancel
[Title scene]
```

## 3. 構成要素 (確定)

| 要素 | 種別 | 責務 | 配置 |
|---|---|---|---|
| `GameLoopAsset` | Authoring data | node / transition / condition の保存 | `Source/GameLoop/GameLoopAsset.h` |
| `GameLoopNode` | Authoring data | 1 scene を表す | 同上 |
| `GameLoopTransition` | Authoring data | node 間遷移を表す | 同上 |
| `GameLoopCondition` | Authoring data | transition 成立条件 | 同上 |
| `GameLoopRuntime` | Runtime state | scene 跨ぎで残る進行状態 | `Source/GameLoop/GameLoopRuntime.h` |
| `GameLoopSystem` | Runtime system | condition 評価 → pending 設定 | `Source/GameLoop/GameLoopSystem.{h,cpp}` |
| `SceneTransitionSystem` | Runtime system | frame 末尾で実 scene load | `Source/GameLoop/SceneTransitionSystem.{h,cpp}` |
| `UIButtonClickSystem` | Runtime system | 2D Button の click 検出と event 発行 | `Source/GameLoop/UIButtonClickSystem.{h,cpp}` |
| `UIButtonComponent` | Component | Button entity を識別 + click event 名 | `Source/Component/UIButtonComponent.h` |
| `UIButtonClickEventQueue` | Runtime queue | 1 frame 分の click event を保持 | `Source/GameLoop/UIButtonClickEventQueue.h` |
| `GameLoopEditorPanel` | Editor UI | asset の編集 / validate / save / load | `Source/GameLoop/GameLoopEditorPanel.{h,cpp}` |
| `GameLoopRegistry` | Persistent registry | persistent input owner と GameLoop 用 entity | `EngineKernel` 所有 |

## 4. 配置とライフタイム (厳格定義)

### 4.1 EngineKernel への組み込み

`EngineKernel` に次のメンバを **必ず** 追加する。

```cpp
// EngineKernel.h (追加)
private:
    GameLoopAsset            m_gameLoopAsset;
    GameLoopRuntime          m_gameLoopRuntime;
    Registry                 m_gameLoopRegistry;        // persistent input owner 用
    UIButtonClickEventQueue  m_uiButtonClickQueue;

public:
    GameLoopAsset&           GetGameLoopAsset()           { return m_gameLoopAsset; }
    GameLoopRuntime&         GetGameLoopRuntime()         { return m_gameLoopRuntime; }
    Registry&                GetGameLoopRegistry()        { return m_gameLoopRegistry; }
    UIButtonClickEventQueue& GetUIButtonClickQueue()      { return m_uiButtonClickQueue; }
```

理由:

- `m_gameLoopRegistry` は scene load の影響を受けない。GameLoop input owner entity をここに作ると persistent になる。
- `m_gameLoopAsset` は Editor で作成・編集される authoring data。
- `m_gameLoopRuntime` は scene 跨ぎ状態。
- `m_uiButtonClickQueue` は 1 frame 分の Button click event を蓄えるバッファ。

### 4.2 ライフタイムまとめ

| データ | 保持元 | scene load で破棄? | EngineMode 切替で破棄? |
|---|---|---|---|
| `GameLoopAsset` | EngineKernel | ✗ | ✗ |
| `GameLoopRuntime` | EngineKernel | ✗ | Stop で reset (§9 参照) |
| `m_gameLoopRegistry` の entity | EngineKernel | ✗ | ✗ |
| `GameLayer::m_registry` の entity | GameLayer | ✓ | ✗ |
| `UIButtonClickEventQueue` | EngineKernel | ✗ (毎 frame 末尾で clear) | ✗ |

## 5. GameLoopAsset (確定)

```cpp
struct GameLoopAsset
{
    uint32_t                          startNodeId = 0;
    std::vector<GameLoopNode>         nodes;
    std::vector<GameLoopTransition>   transitions;
};
```

- `startNodeId` は Play 開始時に最初に load される node の id。
- `nodes` の id は asset 内で **重複不可**。新規 node 生成時は asset 内最大 id + 1 を採番する。
- `transitions` の配列順は **優先度順** (先頭が高優先)。EditorPanel 側で並び替え可能とする。

## 6. GameLoopNode (確定)

```cpp
enum class GameLoopNodeType : uint8_t
{
    Scene = 0,        // v0 で唯一の type
};

struct GameLoopNode
{
    uint32_t           id = 0;
    std::string        name;
    std::string        scenePath;
    GameLoopNodeType   type = GameLoopNodeType::Scene;
};
```

- `name` は authoring 識別用ラベル (Title / Battle / Result 等)。
- `scenePath` は `PrefabSystem::LoadSceneIntoRegistry` が読める path。
- `type` は将来拡張のための予約。v1.1 では `Scene` のみ。

## 7. GameLoopTransition (確定)

```cpp
struct GameLoopTransition
{
    uint32_t                         fromNodeId = 0;
    uint32_t                         toNodeId   = 0;
    std::string                      name;
    std::vector<GameLoopCondition>   conditions;
    bool                             requireAllConditions = true;
};
```

- `requireAllConditions == true` → AND 評価。
- `requireAllConditions == false` → OR 評価。
- `conditions.empty()` は **validate error** (無条件遷移は `GameLoopConditionType::None` を 1 件入れる)。
- `fromNodeId == toNodeId` (self-loop) は **許可**。Result scene を時間経過で再表示する等に利用。
- 同フレームで複数 transition が成立した場合、**配列先頭** を採用。残りは破棄 (次 frame で再評価)。

## 8. GameLoopCondition (確定)

### 8.1 v1.1 で扱う condition type

```cpp
enum class GameLoopConditionType : uint8_t
{
    None              = 0,
    InputPressed      = 1,
    UIButtonClicked   = 2,    // ★ v1.1 で追加
    TimerElapsed      = 3,
    ActorDead         = 4,
    AllActorsDead     = 5,
    ActorMovedDistance= 6,
    RuntimeFlag       = 7,

    // Phase 4 以降:
    ActorEnteredArea  = 100,
    StateMachineState = 101,
    TimelineEvent     = 102,
    CustomEvent       = 103,
};

struct GameLoopCondition
{
    GameLoopConditionType type = GameLoopConditionType::None;

    // 共通使用フィールド (condition 種別ごとに利用)
    ActorType    actorType      = ActorType::None;
    std::string  targetName;        // UIButton id / actor name (将来用)
    std::string  parameterName;     // RuntimeFlag 名
    std::string  eventName;         // 将来用
    int          actionIndex   = -1;
    float        threshold     = 0.0f;
    float        seconds       = 0.0f;
};
```

### 8.2 Condition 別利用フィールド表 (確定)

| Condition | 利用 field | 評価 |
|---|---|---|
| `None` | (なし) | 常に true |
| `InputPressed` | `actionIndex` | persistent input owner の `ResolvedInputStateComponent.actions[actionIndex].pressed == true` |
| `UIButtonClicked` | `targetName` | この frame の `UIButtonClickEventQueue` に `buttonId == targetName` が含まれる |
| `TimerElapsed` | `seconds` | `runtime.nodeTimer >= seconds` |
| `ActorDead` | `actorType` | `GameLayer::m_registry` 内に `ActorTypeComponent.type == actorType` かつ `HealthComponent.isDead == true` の entity が **1 体以上** 存在 |
| `AllActorsDead` | `actorType` | `actorType` の entity が 1 体以上存在し、かつ **全員** `isDead == true` |
| `ActorMovedDistance` | `actorType`, `threshold` | node 開始位置から `threshold` 以上移動 (XY 平面距離) |
| `RuntimeFlag` | `parameterName` | `runtime.flags[parameterName] == true` |

### 8.3 Field 検証

- `actionIndex` は `[0, ResolvedInputStateComponent::MAX_ACTIONS - 1]` の範囲。
- `seconds` は `> 0`。
- `threshold` は `> 0`。
- `actorType` が `ActorType::None` の actor 系 condition は **validate error**。
- `parameterName` が空の `RuntimeFlag` は **validate warning**。

## 9. GameLoopRuntime (確定)

```cpp
struct GameLoopRuntime
{
    uint32_t          currentNodeId  = 0;
    uint32_t          previousNodeId = 0;
    uint32_t          pendingNodeId  = 0;

    std::string       currentScenePath;
    std::string       pendingScenePath;

    bool              sceneTransitionRequested = false;
    bool              waitingSceneLoad         = false;
    bool              forceReload              = false;   // ★ 同一 scene 再 load 用

    float             nodeTimer = 0.0f;

    // ActorMovedDistance 用
    DirectX::XMFLOAT3 observedActorStartPosition{ 0.0f, 0.0f, 0.0f };
    bool              observedActorPositionInitialized = false;

    // RuntimeFlag 用
    std::unordered_map<std::string, bool> flags;

    // GameLoop 状態管理
    bool              isActive = false;        // Play 中で GameLoop 進行中なら true
};
```

- `forceReload` は同一 scene 再 load を要求するときに true にする (Result→Battle Retry など)。
- `flags` は `RuntimeFlag` condition の真偽値テーブル。GameLoop 外からも `runtime.flags[name] = true` で set できる (gameplay 側の Clear シグナル等に使用)。
- `isActive` は Stop で false、Play で true になる。

### 9.1 scene 遷移成功時に必ず実行する処理

```cpp
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
// flags は保持 (scene 跨ぎで残る)
```

### 9.2 scene 遷移失敗時に必ず実行する処理

```cpp
runtime.pendingNodeId              = 0;
runtime.pendingScenePath.clear();
runtime.sceneTransitionRequested   = false;
runtime.waitingSceneLoad           = false;
runtime.forceReload                = false;
// currentNodeId / currentScenePath / nodeTimer は保持 (現 scene に留まる)
```

## 10. EngineMode との連動 (確定)

| EngineMode 遷移 | GameLoopRuntime への影響 |
|---|---|
| `Editor` → `Play` | `runtime.isActive = true`<br>`runtime.currentNodeId = asset.startNodeId`<br>start node の `scenePath` を pending にして `sceneTransitionRequested = true`<br>`runtime.flags.clear()`<br>`runtime.nodeTimer = 0` |
| `Play` → `Pause` | `runtime.isActive = true` を維持<br>`GameLoopSystem::Update` は早期 return (後述) |
| `Pause` → `Play` | (差分なし。`isActive` は既に true) |
| `Play` / `Pause` → `Editor` (Stop) | `runtime.isActive = false`<br>`runtime.currentNodeId = 0`<br>`runtime.previousNodeId = 0`<br>`runtime.pendingNodeId = 0`<br>`runtime.currentScenePath.clear()`<br>`runtime.pendingScenePath.clear()`<br>`runtime.sceneTransitionRequested = false`<br>`runtime.waitingSceneLoad = false`<br>`runtime.nodeTimer = 0`<br>`runtime.flags.clear()`<br>`runtime.observedActorPositionInitialized = false`<br>**Editor 復帰時に scene を元 (起動時の編集 scene) に戻すかは別仕様** (§10.1 参照) |

### 10.1 Stop 時の scene 復帰

Stop 押下時、Editor が編集中だった scene を再 load する責務は **EditorLayer 側** が担う。`GameLoopSystem` / `SceneTransitionSystem` は触れない。

これは既存の `EditorLayer::ExecuteGameStop` フローの責務範囲とする。実装ポイント:

1. Play 押下時、EditorLayer は `m_sceneSavePath` に編集中 scene path を一時退避する。
2. Stop 押下時、退避した path から `LoadSceneFromPath` で復元する。
3. もし退避 path が空であれば `NewScene` を呼ぶ。

## 11. Frame phase の厳格定義

`EngineKernel::Update(float rawDt)` は次の順序で各 system を呼ぶ。

```text
[Phase A: Input poll]
  EngineKernel::PollInput()                ... 既存

[Phase B: GameLayer::Update]
  GameLayer::Update(time)
    InputContextSystem
    InputResolveSystem                      ... persistent owner も resolve する (§13 参照)
    InputTextSystem
    InputFeedbackSystem
    PlayerInputSystem
    PlaybackSystem
    StateMachineSystem
    LocomotionSystem
    StaminaSystem
    HealthSystem
    DodgeSystem
    CharacterPhysicsSystem
    TimelineSystem
    ActionSystem
    CinematicService::Update
    EffectSpawnSystem / PlaybackSystem / SimulationSystem / LifetimeSystem / PreviewSystem
    FreeCameraSystem
    TransformSystem (1 回目)
    AnimatorSystem
    TransformSystem (2 回目)
    ModelUpdateSystem
    NodeAttachmentSystem
    TimelineHitboxSystem / VFXSystem / AudioSystem / ShakeSystem
    EffectAttachmentSystem
    TrailSystem
    HitboxTrackingSystem
    CameraFinalizeSystem

    UIButtonClickSystem                     ... ★ v1.1 で追加
    GameLoopSystem                          ... ★ v1.1 で追加

[Phase C: EndOfFrame]
  SceneTransitionSystem::UpdateEndOfFrame   ... ★ v1.1 で追加
  UIButtonClickEventQueue::Clear            ... ★ v1.1 で追加

[Phase D: Render]
  EngineKernel::Render()                    ... 既存
```

### 11.1 「frame 末尾」の確定定義

「frame 末尾」 = **`GameLayer::Update` 完了後、`EngineKernel::Render` 開始前**。

理由:

- 描画 extraction (`MeshExtractSystem`, `EffectExtractSystem` 等) は `Render` 内で実行される。Registry を `Render` 中に差し替えると extract した command が無効になる。よって `Render` 開始前に scene load を完了させる必要がある。
- `Update` 中に Registry を差し替えると、後続 system (たとえ GameLoopSystem の後続が無くても、次 frame の Update) との整合が取りづらいので、Phase 境界で行う。

### 11.2 同 frame での連鎖遷移を禁止

`SceneTransitionSystem::UpdateEndOfFrame` は **1 frame に最大 1 回の scene load** のみ実行する。`waitingSceneLoad` を判定 → 実 load → `false` に戻す、までを 1 frame 内に完結させ、その frame の `GameLoopSystem` には新 scene の condition 評価をさせない。

新 scene の condition 評価は **次 frame** の `GameLoopSystem::Update` から行う。これにより:

- 1 frame で Title→Battle→Result の連鎖を防ぐ (input pressed が同 frame で複数 node に効くのを回避)
- `ResolvedInputStateComponent.actions[i].pressed` の単発性が保証される

## 12. GameLoopSystem (確定)

```cpp
class GameLoopSystem
{
public:
    static void Update(
        const GameLoopAsset&           asset,
        GameLoopRuntime&               runtime,
        Registry&                      gameRegistry,           // GameLayer::m_registry
        Registry&                      gameLoopRegistry,       // EngineKernel::m_gameLoopRegistry
        const UIButtonClickEventQueue& clickQueue,
        float                          dt);
};
```

### 12.1 処理手順 (確定)

```cpp
void GameLoopSystem::Update(...)
{
    // 1. 進行可能性の判定
    if (!runtime.isActive)                       return;
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) return;
    if (runtime.waitingSceneLoad)                return;
    if (runtime.sceneTransitionRequested)        return;

    // 2. nodeTimer 更新
    runtime.nodeTimer += dt;

    // 3. ActorMovedDistance 用の初期位置記録
    InitializeObservedActorIfNeeded(runtime, gameRegistry);

    // 4. current node の transition を配列順に走査
    for (const auto& transition : asset.transitions) {
        if (transition.fromNodeId != runtime.currentNodeId) continue;
        if (transition.conditions.empty())                  continue;  // validate で弾かれる前提だが防御

        if (!EvaluateTransition(transition, runtime, gameRegistry, gameLoopRegistry, clickQueue))
            continue;

        const GameLoopNode* toNode = FindNode(asset, transition.toNodeId);
        if (!toNode) return;

        runtime.pendingNodeId            = toNode->id;
        runtime.pendingScenePath         = toNode->scenePath;
        runtime.sceneTransitionRequested = true;
        runtime.forceReload              = (toNode->id == runtime.currentNodeId);
        return;  // 1 frame 1 transition のみ
    }
}
```

### 12.2 EvaluateTransition

```cpp
bool EvaluateTransition(...)
{
    if (transition.requireAllConditions) {
        for (auto& c : transition.conditions)
            if (!EvaluateCondition(c, runtime, ...)) return false;
        return true;
    } else {
        for (auto& c : transition.conditions)
            if (EvaluateCondition(c, runtime, ...)) return true;
        return false;
    }
}
```

### 12.3 EvaluateCondition (条件別)

| Type | 評価ロジック |
|---|---|
| `None` | `return true;` |
| `InputPressed` | `gameLoopRegistry` の persistent input owner entity の `ResolvedInputStateComponent.actions[c.actionIndex].pressed` |
| `UIButtonClicked` | `clickQueue.Contains(c.targetName)` |
| `TimerElapsed` | `runtime.nodeTimer >= c.seconds` |
| `ActorDead` | `gameRegistry` を `Query<ActorTypeComponent, HealthComponent>` で走査し、`type == c.actorType && isDead` が 1 件以上あれば true |
| `AllActorsDead` | 上記 query で `type == c.actorType` の総数 >= 1 かつ全員 `isDead` |
| `ActorMovedDistance` | `gameRegistry` で `type == c.actorType` の先頭 entity を取り、`worldPosition` と `runtime.observedActorStartPosition` の XZ 平面距離が `c.threshold` 以上 |
| `RuntimeFlag` | `runtime.flags.count(c.parameterName) && runtime.flags[c.parameterName]` |

### 12.4 GameLoopSystem の禁止事項

- `PrefabSystem::LoadSceneIntoRegistry` を直接呼ばない
- `Registry` を直接 clear しない
- `EditorLayer` を直接操作しない
- `runtime.currentNodeId` の更新を `SceneTransitionSystem` 以外で行わない
- `clickQueue` を **書き換え** ない (read only)

## 13. Persistent Input Owner (確定)

GameLoop 用の input action map は **`m_gameLoopRegistry` 内** に作る persistent entity が所有する。これは scene 跨ぎで保持される。

### 13.1 生成タイミング

`EngineKernel::Initialize()` の最後で 1 度だけ生成する。

```cpp
EntityID owner = m_gameLoopRegistry.CreateEntity();
m_gameLoopRegistry.AddComponent(owner, NameComponent{ "GameLoopInputOwner" });
m_gameLoopRegistry.AddComponent(owner, InputUserComponent{ /* userId=0 */ });
m_gameLoopRegistry.AddComponent(owner, InputBindingComponent{ /* default profile */ });
m_gameLoopRegistry.AddComponent(owner, InputContextComponent{ /* GameLoop 用 context */ });
m_gameLoopRegistry.AddComponent(owner, InputActionMapComponent{
    InputActionMapAsset{
        .name = "GameLoop",
        .actions = {
            { "Confirm", /*scancode=Enter*/ 40, 0, /*gamepadA*/ 0, ... },
            { "Cancel",  /*scancode=Esc  */ 41, 0, /*gamepadB*/ 1, ... },
            { "Retry",   /*scancode=R    */ 21, 0, /*gamepadX*/ 2, ... },
        }
    }
});
m_gameLoopRegistry.AddComponent(owner, ResolvedInputStateComponent{});
```

### 13.2 Input Resolve の経路

`InputResolveSystem::Update` は `GameLayer::m_registry` 専用ではなく、**毎 frame 両方の Registry に対して実行** する必要がある。

実装:

```cpp
// GameLayer::Update 内
InputResolveSystem::Update(m_registry, eventQueue, time.unscaledDt);
InputResolveSystem::Update(EngineKernel::Instance().GetGameLoopRegistry(),
                           eventQueue, time.unscaledDt);
```

### 13.3 Action Index の固定

| Index | Action 名 | 既定 binding |
|---|---|---|
| 0 | `Confirm` | Enter / Space / Gamepad A |
| 1 | `Cancel`  | Escape / Backspace / Gamepad B |
| 2 | `Retry`   | R / Gamepad X |

`InputActionMapAsset.actions` の登録順がそのまま `actionIndex` に対応する。EditorPanel で並び順を変えると condition の参照先が変わるため、`GameLoop` action map の並び順は **固定** とする (validate で警告を出す)。

## 14. 2D UI Button 連携 (新規 — v1.1 の中核)

### 14.1 既存 UI 機構の利用

エンジンには既に次が存在する:

- [`Source/Component/CanvasItemComponent.h`](Source/Component/CanvasItemComponent.h) — `visible` / `interactable` / `sortingLayer` / `orderInLayer`
- [`Source/Component/RectTransformComponent.h`](Source/Component/RectTransformComponent.h) — `anchoredPosition` / `sizeDelta` / `pivot`
- [`Source/Component/SpriteComponent.h`](Source/Component/SpriteComponent.h) — `textureAssetPath` / `tint`
- [`Source/UI/UIHitTestSystem.h`](Source/UI/UIHitTestSystem.h) — `PickTopmost(registry, viewRect, view, projection, screenPoint)` で screen 座標から最上位 entity を返す

これらを **再利用** する。Button 用には新しい component を 1 つだけ追加する。

### 14.2 新規 component: `UIButtonComponent`

```cpp
// Source/Component/UIButtonComponent.h
struct UIButtonComponent
{
    std::string buttonId;        // 一意な ID。GameLoopCondition.targetName と一致させる
    bool        enabled = true;  // false なら click event を発行しない
    // 演出 (将来):
    // float     hoverScale = 1.0f;
    // XMFLOAT4  pressedTint = { 0.7f, 0.7f, 0.7f, 1.0f };
};
```

Button entity は **次の component セット** を持つ (Hierarchy 上の prefab で組む):

- `TransformComponent`
- `RectTransformComponent`
- `CanvasItemComponent` (`interactable = true`)
- `SpriteComponent`
- `UIButtonComponent`
- (任意) `HierarchyComponent`

### 14.3 click 検出: `UIButtonClickSystem`

```cpp
// Source/GameLoop/UIButtonClickSystem.h
class UIButtonClickSystem
{
public:
    static void Update(
        Registry&                gameRegistry,
        UIButtonClickEventQueue& outQueue,
        const InputEventQueue&   inputQueue,
        const DirectX::XMFLOAT4& gameViewRect,
        const DirectX::XMFLOAT4X4& view2D,
        const DirectX::XMFLOAT4X4& projection2D);
};
```

処理:

1. `inputQueue` から `MouseButtonDown(left)` event を抽出する。
2. event の screen 座標を `UIHitTestSystem::PickTopmost` で 2D pick する。
3. hit 結果 entity が `UIButtonComponent` を持ち、`enabled == true` なら `outQueue.Push(buttonId)` を呼ぶ。
4. Touch event も同様に扱う (将来拡張)。

### 14.4 `UIButtonClickEventQueue`

```cpp
class UIButtonClickEventQueue
{
public:
    void   Push(const std::string& buttonId);
    bool   Contains(const std::string& buttonId) const;
    void   Clear();   // frame 末尾で呼ばれる

    const std::vector<std::string>& GetAll() const;

private:
    std::vector<std::string> m_clickedButtonIds;
};
```

- `Contains` は `GameLoopSystem` の `UIButtonClicked` 評価で使われる。
- `Clear` は `EngineKernel` が frame 末尾 (Phase C) で必ず呼ぶ。

### 14.5 GameLoopCondition との連動

```cpp
GameLoopCondition c;
c.type       = GameLoopConditionType::UIButtonClicked;
c.targetName = "StartButton";
```

これを `Title -> Battle` の transition に設定すれば、Title scene 上の `buttonId == "StartButton"` を持つ Button click で Battle に遷移する。

### 14.6 Game View の view/projection 取得

`UIButtonClickSystem` は 2D view/projection を必要とする。これは `GameLayer` 側で `Camera2DComponent` (または既存の Game view camera) から構築する。

実装方針:

- `EditorLayer::GetGameViewRect()` で screen 上の Game view rect を取得。
- `Camera2DComponent` を持つ entity の matrix を取得 (既存 `CameraFinalizeSystem` で計算済み)。
- これらを `UIButtonClickSystem::Update` に渡す。

ヘッドレス (Game build) の場合は window rect 全体を使う。

## 15. SceneTransitionSystem (確定)

```cpp
class SceneTransitionSystem
{
public:
    static void UpdateEndOfFrame(
        GameLoopRuntime& runtime,
        Registry&        gameRegistry);
};
```

### 15.1 処理手順 (確定)

```cpp
void SceneTransitionSystem::UpdateEndOfFrame(GameLoopRuntime& runtime, Registry& gameRegistry)
{
    if (!runtime.sceneTransitionRequested) return;

    // 1. pending path validation
    if (runtime.pendingScenePath.empty()) {
        runtime.pendingNodeId = 0;
        runtime.sceneTransitionRequested = false;
        runtime.waitingSceneLoad = false;
        runtime.forceReload = false;
        // log error
        return;
    }

    // 2. 同一 scene 再 load の判定
    const bool sameScene = (runtime.pendingScenePath == runtime.currentScenePath);
    if (sameScene && !runtime.forceReload) {
        // node id は変えるが scene 再 load しない (self-loop など)
        runtime.previousNodeId           = runtime.currentNodeId;
        runtime.currentNodeId            = runtime.pendingNodeId;
        runtime.pendingNodeId            = 0;
        runtime.pendingScenePath.clear();
        runtime.sceneTransitionRequested = false;
        runtime.waitingSceneLoad         = false;
        runtime.forceReload              = false;
        runtime.nodeTimer                = 0.0f;
        runtime.observedActorPositionInitialized = false;
        return;
    }

    // 3. load 開始
    runtime.waitingSceneLoad = true;
    EngineKernel::Instance().ResetRenderStateForSceneChange();

    const bool ok = PrefabSystem::LoadSceneIntoRegistry(runtime.pendingScenePath, gameRegistry);

    // 4. 結果反映
    if (!ok) {
        // 失敗時 (§9.2)
        runtime.pendingNodeId = 0;
        runtime.pendingScenePath.clear();
        runtime.sceneTransitionRequested = false;
        runtime.waitingSceneLoad = false;
        runtime.forceReload = false;
        // log error / debug UI 表示
        return;
    }

    // 成功時 (§9.1)
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
}
```

### 15.2 SceneTransitionSystem の禁止事項

- `GameLoopAsset` を読まない
- condition 評価をしない
- `pendingScenePath` を自分で書き換えない (§15.1 step 2 のクリアのみ許可)
- 1 frame で複数回呼ばれない (`EngineKernel` が 1 度だけ呼ぶ)

## 16. EngineKernel::Play / Stop の実装変更点

### 16.1 Play

```cpp
void EngineKernel::Play()
{
    if (mode == EngineMode::Play) return;

    // (既存) editor scene の退避は EditorLayer 側
    mode = EngineMode::Play;

    // GameLoop 起動
    GameLoopRuntime& rt = m_gameLoopRuntime;
    rt.isActive                          = true;
    rt.flags.clear();
    rt.nodeTimer                         = 0.0f;
    rt.observedActorPositionInitialized  = false;
    rt.previousNodeId                    = 0;

    if (m_gameLoopAsset.nodes.empty()) {
        // asset 未設定なら GameLoop 機能を実質オフ (現 scene のまま遊ぶ)
        rt.isActive       = false;
        rt.currentNodeId  = 0;
        rt.currentScenePath.clear();
        return;
    }

    const GameLoopNode* startNode = FindNode(m_gameLoopAsset, m_gameLoopAsset.startNodeId);
    if (!startNode) {
        // start 不正
        rt.isActive = false;
        // log error
        return;
    }

    rt.currentNodeId             = 0;     // 一度クリア
    rt.currentScenePath.clear();
    rt.pendingNodeId             = startNode->id;
    rt.pendingScenePath          = startNode->scenePath;
    rt.sceneTransitionRequested  = true;
    rt.forceReload               = true;  // Play 押下時は確実に最初の scene を load する
}
```

### 16.2 Stop

```cpp
void EngineKernel::Stop()
{
    if (mode == EngineMode::Editor) return;

    mode = EngineMode::Editor;

    // GameLoop 停止 (§10 表に従う)
    GameLoopRuntime& rt = m_gameLoopRuntime;
    rt.isActive                          = false;
    rt.currentNodeId                     = 0;
    rt.previousNodeId                    = 0;
    rt.pendingNodeId                     = 0;
    rt.currentScenePath.clear();
    rt.pendingScenePath.clear();
    rt.sceneTransitionRequested          = false;
    rt.waitingSceneLoad                  = false;
    rt.forceReload                       = false;
    rt.nodeTimer                         = 0.0f;
    rt.observedActorPositionInitialized  = false;
    rt.flags.clear();

    // (既存) editor scene の復元は EditorLayer 側
}
```

### 16.3 Pause / Step

挙動変更なし。`GameLoopSystem::Update` は `mode == Play` のときのみ進行する (§12 step 1)。`Pause` 中は早期 return。

## 17. EditorLayer 側の変更点

### 17.1 Play 押下時の scene 退避

```cpp
// EditorLayer::ExecuteGamePlay
m_savedEditorScenePath = m_sceneSavePath;  // 退避
EngineKernel::Instance().Play();
```

### 17.2 Stop 押下時の scene 復元

```cpp
// EditorLayer::ExecuteGameStop
EngineKernel::Instance().Stop();
if (!m_savedEditorScenePath.empty()) {
    LoadSceneFromPath(m_savedEditorScenePath);
} else {
    NewScene();
}
m_savedEditorScenePath.clear();
```

### 17.3 GameLoopAsset の load / save

EditorLayer 起動時に `Data/GameLoop/Main.gameloop` を read し、`EngineKernel::Instance().GetGameLoopAsset()` に格納する。存在しなければ default loop を生成して warning を出す。

## 18. GameLoopEditorPanel (確定 v1.1)

### 18.1 構成

```text
┌──────────────────────────────────────────┐
│ Toolbar:                                  │
│   [New Default Loop] [Load] [Save]        │
│   [Save As] [Validate]                    │
├───────────────────┬──────────────────────┤
│ Node List         │ Node Inspector       │
│   [+] [-] [↑] [↓] │   name [______]      │
│   * Title (start) │   scenePath [...]    │
│   - Battle        │   [Browse...]        │
│   - Result        │   [☆] Set as Start   │
│                   │                      │
│ Transition List   │ Transition Inspector │
│   [+] [-] [↑] [↓] │   from [Title▾]      │
│   - Title→Battle  │   to   [Battle▾]     │
│   - Battle→Result │   [✓] requireAll     │
│   - Result→Battle │   Conditions:        │
│   - Result→Title  │     [+] [-]          │
│                   │     - UIButtonClicked│
│                   │       targetName     │
│                   │       [StartButton]  │
├───────────────────┴──────────────────────┤
│ Validate Result:                          │
│   [E] startNodeId が存在しない          │
│   [W] Battle に出口 transition がない    │
└──────────────────────────────────────────┘
```

### 18.2 機能要件

| 機能 | 動作 |
|---|---|
| `New Default Loop` | §22 標準サンプル loop を生成 |
| `Load` | `Data/GameLoop/*.gameloop` から JSON 読込 |
| `Save` | 現在の path に JSON 書出 |
| `Save As` | path 指定で JSON 書出 |
| `Validate` | §19 validate 規則を実行 → result panel に表示 |
| Node 追加 | id 採番 (max+1)、空 name / scenePath で追加 |
| Node 削除 | 関連 transition も削除確認 dialog 表示 |
| Node 並べ替え | UI 上の表示順のみ。runtime には影響しない |
| Transition 追加 | from/to dropdown は既存 node から選択 |
| Transition 並べ替え | **配列順 = 優先度** (§7) |
| Condition 追加 | type を dropdown で選択、type に応じた field を出し分け |
| Set as Start | 選択 node を `startNodeId` にセット |
| Play 中表示 | current node を太字 + ハイライト、nodeTimer / pending 表示 |

### 18.3 Condition Editor の field 出し分け

| Type | UI |
|---|---|
| `None` | (フィールドなし) |
| `InputPressed` | `actionIndex` int input + ヒント (`0=Confirm, 1=Cancel, 2=Retry`) |
| `UIButtonClicked` | `targetName` text input |
| `TimerElapsed` | `seconds` float input (> 0) |
| `ActorDead` | `actorType` dropdown |
| `AllActorsDead` | `actorType` dropdown |
| `ActorMovedDistance` | `actorType` dropdown + `threshold` float (> 0) |
| `RuntimeFlag` | `parameterName` text input |

### 18.4 既存 panel との統合

- `EditorLayer` に `m_showGameLoopEditor = false` フラグを追加。
- Menu bar `Window > GameLoop Editor` で toggle。
- `WindowFocusTarget` に `GameLoopEditor` を追加。
- 既存 `SequencerPanel` / `PlayerEditorPanel` / `EffectEditorPanel` と同列の panel として扱う。

## 19. Validate 仕様 (確定)

### 19.1 実行タイミング

| トリガ | 実行内容 |
|---|---|
| `[Validate]` ボタン | 全項目実行、結果 panel に表示 |
| `Save` 実行時 | 全項目実行、Error が 1 件でもあれば中止 + dialog |
| `Play` 押下時 | 全項目実行、Error が 1 件でもあれば中止 + dialog |
| node / transition / condition 編集時 | (将来) リアルタイム部分 validate。v1.1 では未実装でよい |

### 19.2 Error 規則 (修正必須)

- `startNodeId` が `nodes` に存在しない
- node id が重複
- node `name` が空
- node `scenePath` が空
- node `scenePath` のファイルが存在しない (実行ファイル基準)
- transition `fromNodeId` が存在しない
- transition `toNodeId` が存在しない
- transition `conditions.empty()`
- `InputPressed` の `actionIndex` が `[0, 31]` 外
- `UIButtonClicked` の `targetName` が空
- `TimerElapsed` の `seconds <= 0`
- `ActorMovedDistance` の `threshold <= 0`
- actor 系 condition の `actorType == ActorType::None`

### 19.3 Warning 規則 (要確認)

- node が出口 transition を 1 つも持たない (dead-end)
- 到達不能な node (start から graph traverse で届かない)
- 同一 from/to の transition が複数 (常に先頭しか発火しない)
- `RuntimeFlag` の `parameterName` が空
- GameLoop input owner の action map 並び順が `Confirm, Cancel, Retry` でない

### 19.4 Info

- 各 node の出口 transition 数
- 各 transition の condition 数
- start からの到達順序

## 20. Serialization 形式 (確定)

`.gameloop` は **JSON 形式**。`nlohmann::json` を使う (既存 `InputActionMapAsset` と同方針)。

```json
{
  "version": 1,
  "startNodeId": 1,
  "nodes": [
    { "id": 1, "name": "Title",  "scenePath": "Data/Scenes/Title.scene",  "type": "Scene" },
    { "id": 2, "name": "Battle", "scenePath": "Data/Scenes/Battle.scene", "type": "Scene" },
    { "id": 3, "name": "Result", "scenePath": "Data/Scenes/Result.scene", "type": "Scene" }
  ],
  "transitions": [
    {
      "fromNodeId": 1, "toNodeId": 2, "name": "Title to Battle",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "StartButton" },
        { "type": "InputPressed", "actionIndex": 0 }
      ]
    },
    {
      "fromNodeId": 2, "toNodeId": 3, "name": "Battle Clear",
      "requireAllConditions": true,
      "conditions": [
        { "type": "AllActorsDead", "actorType": "Enemy" }
      ]
    },
    {
      "fromNodeId": 3, "toNodeId": 2, "name": "Result Retry",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "RetryButton" },
        { "type": "InputPressed", "actionIndex": 2 }
      ]
    },
    {
      "fromNodeId": 3, "toNodeId": 1, "name": "Result Title",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "TitleButton" },
        { "type": "InputPressed", "actionIndex": 1 }
      ]
    }
  ]
}
```

`version` は破壊的変更があったとき incremented。読み込み時に互換性チェックを行う。

## 21. 実装フェーズ (v1.1 改訂版)

### Phase 1: Runtime 固定ループ (最小動作)

目標: hardcoded asset で Title→Battle→Result→Retry を runtime で通す。

完成条件:

- `Source/GameLoop/` 以下を新規作成
- `EngineKernel` に `m_gameLoopAsset / m_gameLoopRuntime / m_gameLoopRegistry / m_uiButtonClickQueue` 追加
- `GameLayer::Update` 末尾に `UIButtonClickSystem` / `GameLoopSystem` 追加
- `EngineKernel::Update` 末尾 (Render 前) に `SceneTransitionSystem::UpdateEndOfFrame` / `UIButtonClickEventQueue::Clear` 追加
- `EngineKernel::Play / Stop` を §16 のとおり書き換え
- `UIButtonComponent` 追加、ComponentMeta 自動生成に登録
- 標準 Title / Battle / Result scene を `Data/Scenes/` に作成 (内容は最小: Player + 出口条件)
- Title scene に Start ボタン (Sprite + RectTransform + UIButton "StartButton") を配置
- Result scene に Retry / Title ボタン配置
- Editor から Play 押下 → Title 表示 → StartButton クリック → Battle → Clear 条件 → Result → Retry click → Battle 再 load を確認

### Phase 2: GameLoopAsset の JSON 化

目標: hardcoded asset を `.gameloop` 読み込みに置き換える。

完成条件:

- `GameLoopAsset::LoadFromFile / SaveToFile` 実装
- `Data/GameLoop/Main.gameloop` を起動時に load
- load 失敗時は default loop を生成 + warning

### Phase 3: GameLoopEditorPanel

目標: §18 の panel を実装。

完成条件:

- list UI で node / transition / condition を編集可能
- Validate 結果が UI に表示
- Play 中の current node ハイライト
- Save / Load / Save As / New Default Loop ボタン

### Phase 4: Condition 拡張

目標: §8 の Phase 4 候補 condition を実装。

完成条件:

- `ActorEnteredArea` (Trigger volume と連動)
- `StateMachineState` (`StateMachineSystem` と連動)
- `TimelineEvent` (`TimelineSystem` と連動)
- `CustomEvent` (script 側から `runtime.flags` 経由で発火)
- 未実装 type は false 扱い + warning

### Phase 5 (将来): UX 強化

- Async scene load + loading screen
- Fade in / out transition
- Persistent actor carry-over
- Save data 化
- Node graph editor

## 22. 標準サンプル loop (v1.1)

```json
{
  "version": 1,
  "startNodeId": 1,
  "nodes": [
    { "id": 1, "name": "Title",  "scenePath": "Data/Scenes/Title.scene",  "type": "Scene" },
    { "id": 2, "name": "Battle", "scenePath": "Data/Scenes/Battle.scene", "type": "Scene" },
    { "id": 3, "name": "Result", "scenePath": "Data/Scenes/Result.scene", "type": "Scene" }
  ],
  "transitions": [
    { "fromNodeId": 1, "toNodeId": 2, "name": "Start",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "StartButton" },
        { "type": "InputPressed", "actionIndex": 0 }
      ] },
    { "fromNodeId": 2, "toNodeId": 3, "name": "Clear",
      "requireAllConditions": false,
      "conditions": [
        { "type": "AllActorsDead",      "actorType": "Enemy" },
        { "type": "ActorMovedDistance", "actorType": "Player", "threshold": 1.0 }
      ] },
    { "fromNodeId": 3, "toNodeId": 2, "name": "Retry",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "RetryButton" },
        { "type": "InputPressed", "actionIndex": 2 }
      ] },
    { "fromNodeId": 3, "toNodeId": 1, "name": "Back to Title",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "TitleButton" },
        { "type": "InputPressed", "actionIndex": 1 }
      ] }
  ]
}
```

## 23. 受け入れ条件 (v1.1)

Phase 1 完了時点で次が成立すること:

- [ ] Editor の Play ボタン押下で Title scene が自動 load される
- [ ] Title scene の Start ボタン click で Battle scene へ遷移する
- [ ] Title scene で Confirm 入力 (Enter) でも Battle へ遷移する
- [ ] Battle scene で Clear 条件達成で Result scene へ遷移する
- [ ] Result scene の Retry ボタン click で Battle scene が再 load される
- [ ] Result scene の Title ボタン click で Title scene へ戻れる
- [ ] Stop 押下で Editor 編集 scene に戻る
- [ ] Pause 中は GameLoop が進行しない
- [ ] scene 跨ぎで描画状態が崩れない
- [ ] `GameLoopRuntime` は scene load を跨いで保持される
- [ ] `GameLoopSystem` は scene load API を直接呼ばない
- [ ] `SceneTransitionSystem` は condition 評価を行わない
- [ ] scene load は frame 末尾 (Render 前) で行われる
- [ ] 1 frame に最大 1 回しか scene 遷移が起きない (連鎖遷移なし)
- [ ] GameLoop 入力は persistent owner entity が処理する (scene 跨ぎで死なない)
- [ ] 2D Button click は `UIButtonClickEventQueue` 経由で `GameLoopSystem` に届く
- [ ] `Data/GameLoop/Main.gameloop` (Phase 2 で) save / load 後も graph が保たれる

## 24. 禁止事項 (v1.1 統合)

- 既存の Player gameplay runtime pipeline 仕様と混同すること
- `GameLoopSystem` から `PrefabSystem::LoadSceneIntoRegistry` を直接呼ぶこと
- `GameLoopSystem` から `Registry` を直接 clear すること
- `GameLoopSystem` から `EditorLayer` を直接操作すること
- `GameLoopSystem` から `UIButtonClickEventQueue` を書き換えること (read only)
- scene load と次 scene 決定 logic を同じ system に混ぜること
- Title / Battle / Result を専用 C++ class として固定すること
- Battle 勝敗専用の hard-coded condition にすること
- `PlayerTagComponent` を GameLoop actor 判定の source of truth にすること
- `InputEventQueue` を `GameLoopSystem` が直接読むこと (`UIButtonClickSystem` のみ許可)
- `GameLoopRuntime` を `GameLayer::m_registry` の entity component として持たせること
- frame 途中で `Registry` を差し替えること
- `EngineKernel::Render` 中に scene load を実行すること
- 1 frame で複数の scene 遷移を実行すること
- v1.1 からノードグラフ UI を必須にすること

## 25. 将来拡張 (v1.0 §21 + v1.1 追記)

- `ActorEnteredArea` (Trigger volume)
- `StateMachineState`
- `TimelineEvent`
- `CustomEvent`
- async scene load
- fade in / fade out transition
- loading screen
- scene additive load
- persistent actor carry-over
- checkpoint / continue
- `GameLoopRuntime` の save data 化
- debug overlay (current node / pending node / condition result)
- node graph editor
- sub loop / nested loop
- DLC / chapter / stage select 用の複数 GameLoopAsset
- **Button hover / pressed の演出 (`UIButtonComponent` 拡張)**
- **gamepad / keyboard による Button focus 移動 (UI navigation)**

## 26. 守るべき分離 (v1.1)

将来拡張しても次の分離は **絶対に維持** する。

| 役割 | 担当 |
|---|---|
| GameLoop authoring data | `GameLoopAsset` |
| 次 scene を決める | `GameLoopSystem` |
| scene を load する | `SceneTransitionSystem` |
| scene を跨いで残る状態 | `GameLoopRuntime` |
| persistent input | `m_gameLoopRegistry` 内 owner entity |
| 2D Button click 検出 | `UIButtonClickSystem` |
| 2D Button click 配信 | `UIButtonClickEventQueue` |
| Actor 種別判定 | `ActorTypeComponent` |
| Action 入力判定 | `ResolvedInputStateComponent` |
| EngineMode 切替の起点 | `EngineKernel::Play / Stop / Pause` |
| Editor scene の退避 / 復元 | `EditorLayer` |
