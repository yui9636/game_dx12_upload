# GameLoop Scene Transition 仕様書

作成日: 2026-04-26

## 1. 目的

この仕様書の目的は、既に量産できる scene を任意条件で接続し、ゲーム進行を作るための GameLoop 作成システムを定義することである。

ここで扱う GameLoop は、Player の内部 gameplay runtime pipeline ではない。対象は、Title scene、Battle scene、Result scene、Retry、scene 間 transition、transition condition を使って、ゲーム全体の進行を制御する仕組みである。

例:

```text
Title
  ↓ Confirm
Battle
  ↓ Player が 1.0 unit 歩く
Result
  ↓ Retry
Battle
```

GameLoop は勝敗専用の BattleLoop ではない。条件は `InputPressed`、`TimerElapsed`、`ActorMovedDistance`、`ActorDead`、`AllActorsDead`、`RuntimeFlag` などへ拡張できる汎用的な scene 進行システムとする。

## 2. 非目的

この仕様書では次を扱わない。

- PlayerEditor / Prefab / StateMachine / Locomotion / Timeline / Animation の内部 gameplay runtime pipeline
- Player の攻撃、回避、コンボ、hitbox、VFX、audio の実行仕様
- scene 保存 / 読み込み機構そのものの置き換え
- `PrefabSystem::SaveRegistryAsScene` / `PrefabSystem::LoadSceneIntoRegistry` の再設計
- Battle 専用の勝敗ロジック固定実装
- 最初からノードグラフ式の GameLoop editor を作ること

既存の scene load 経路を利用し、その上に scene 進行の authoring / runtime layer を追加する。

## 3. 基本方針

Unity の `SceneManager` と Unreal Engine の `GameInstance` を混ぜたような設計にする。

- `SceneTransitionSystem` は Unity の `SceneManager` 的に、実際の scene load を担当する。
- `GameLoopRuntime` は Unreal Engine の `GameInstance` 的に、scene をまたいで残る進行状態を保持する。
- `GameLoopSystem` は現在 node の transition condition を評価し、次に読む scene を決める。
- `GameLoopAsset` は editor で作成・保存される authoring data とする。

重要な分離:

- 次の scene を決める logic は `GameLoopSystem` が担当する。
- 実際に scene を load する処理は `SceneTransitionSystem` が担当する。
- `GameLoopSystem` は `Registry` を clear しない。
- `GameLoopSystem` は `PrefabSystem::LoadSceneIntoRegistry` を直接呼ばない。
- `SceneTransitionSystem` は transition condition を評価しない。

GameLoop は既存の scene load 経路を置き換えない。既に存在する次の仕組みを利用する。

- `PrefabSystem::SaveRegistryAsScene`
- `PrefabSystem::LoadSceneIntoRegistry`
- EditorLayer 側の scene load 経路
- `Registry` を使った scene 管理

## 4. 全体構成

GameLoop は単一の C++ クラス名ではなく、scene 進行全体を指す。

構成要素:

| 要素 | 種別 | 責務 |
| --- | --- | --- |
| `GameLoopAsset` | Authoring data | scene node と transition の保存 |
| `GameLoopNode` | Authoring data | 1 つの scene を表す |
| `GameLoopTransition` | Authoring data | node から node への遷移を表す |
| `GameLoopCondition` | Authoring data | transition 成立条件を表す |
| `GameLoopRuntime` | Runtime state | scene をまたいで残る進行状態 |
| `GameLoopSystem` | Runtime system | condition を評価し、pending scene を決める |
| `SceneTransitionSystem` | Runtime system | frame 末尾で実際の scene load を行う |
| `GameLoopEditorPanel` | Editor UI | asset の編集、保存、validate |

概念図:

```text
GameLoopAsset
  nodes / transitions / conditions
      |
      v
GameLoopSystem
  evaluate current node transitions
      |
      v
GameLoopRuntime
  pendingNodeId / pendingScenePath / sceneTransitionRequested
      |
      v
SceneTransitionSystem
  load pending scene through existing scene load path
      |
      v
GameLoopRuntime
  currentNodeId / currentScenePath updated
```

## 5. GameLoopAsset

`GameLoopAsset` は scene node と transition を保存する authoring data である。

必要なデータ:

- `startNodeId`
- `nodes`
- `transitions`

Title / Battle / Result は特別な C++ class にしない。基本的にはすべて `GameLoopNode` として扱い、意味は `name` で表現する。

想定構造:

```cpp
struct GameLoopAsset
{
    uint32_t startNodeId = 0;
    std::vector<GameLoopNode> nodes;
    std::vector<GameLoopTransition> transitions;
};
```

保存形式は Phase 2 で `.gameloop` とする。標準 path は `Data/GameLoop/Main.gameloop` とする。

## 6. GameLoopNode

`GameLoopNode` は 1 scene を表す。

持つ値:

- `id`
- `name`
- `scenePath`
- `type`

v0 の `type` は `Scene` のみでよい。Title / Battle / Result の意味は `name` で表現する。

想定構造:

```cpp
enum class GameLoopNodeType : uint8_t
{
    Scene = 0,
};

struct GameLoopNode
{
    uint32_t id = 0;
    std::string name;
    std::string scenePath;
    GameLoopNodeType type = GameLoopNodeType::Scene;
};
```

`scenePath` は既存 scene load 経路が読める path とする。

例:

- `Data/Scenes/Title.scene`
- `Data/Scenes/Battle.scene`
- `Data/Scenes/Result.scene`

## 7. GameLoopTransition

`GameLoopTransition` は node から node への遷移を表す。

持つ値:

- `fromNodeId`
- `toNodeId`
- `name`
- `conditions`
- `requireAllConditions`

`requireAllConditions == true` の場合は AND 条件とする。すべての condition が成立したときだけ transition が成立する。

`requireAllConditions == false` の場合は OR 条件とする。いずれかの condition が成立したとき transition が成立する。

複数 transition が同時成立した場合は、`GameLoopAsset.transitions` 配列上の先頭を優先する。

想定構造:

```cpp
struct GameLoopTransition
{
    uint32_t fromNodeId = 0;
    uint32_t toNodeId = 0;
    std::string name;
    std::vector<GameLoopCondition> conditions;
    bool requireAllConditions = true;
};
```

condition が 0 件の transition は validate error とする。無条件遷移が必要な場合は `GameLoopConditionType::None` を 1 件入れる。

## 8. GameLoopCondition

`GameLoopCondition` は transition の成立条件である。

v0 で扱う condition type:

- `None`
- `InputPressed`
- `TimerElapsed`
- `ActorDead`
- `AllActorsDead`
- `ActorMovedDistance`
- `RuntimeFlag`

将来候補:

- `ActorEnteredArea`
- `StateMachineState`
- `TimelineEvent`
- `CustomEvent`

Condition は必要に応じて次を持つ。

- `actorType`
- `targetName`
- `parameterName`
- `eventName`
- `actionIndex`
- `threshold`
- `seconds`

想定構造:

```cpp
enum class GameLoopConditionType : uint8_t
{
    None = 0,
    InputPressed,
    TimerElapsed,
    ActorDead,
    AllActorsDead,
    ActorMovedDistance,
    RuntimeFlag,

    ActorEnteredArea,
    StateMachineState,
    TimelineEvent,
    CustomEvent,
};

struct GameLoopCondition
{
    GameLoopConditionType type = GameLoopConditionType::None;
    ActorType actorType = ActorType::None;
    std::string targetName;
    std::string parameterName;
    std::string eventName;
    int actionIndex = -1;
    float threshold = 0.0f;
    float seconds = 0.0f;
};
```

Condition ごとの利用 field:

| Condition | 使用 field | 意味 |
| --- | --- | --- |
| `None` | なし | 常に成立 |
| `InputPressed` | `actionIndex` | GameLoop input action が押された |
| `TimerElapsed` | `seconds` | node 開始から指定秒数が経過 |
| `ActorDead` | `actorType` | 指定 ActorType の actor のいずれかが dead |
| `AllActorsDead` | `actorType` | 指定 ActorType の actor が全員 dead |
| `ActorMovedDistance` | `actorType`, `threshold` | node 開始時から指定距離以上動いた |
| `RuntimeFlag` | `parameterName` | runtime flag が true |

## 9. GameLoopRuntime

`GameLoopRuntime` は scene をまたいで残る runtime state である。

`Registry` 内の通常 scene entity として保持しない。scene load で破棄されない persistent 領域に保持する。候補は `EngineKernel` 側、または scene load の `Registry` clear 対象外となる engine runtime context である。

持つ値:

- `currentNodeId`
- `previousNodeId`
- `pendingNodeId`
- `currentScenePath`
- `pendingScenePath`
- `sceneTransitionRequested`
- `waitingSceneLoad`
- `nodeTimer`
- `observedActorStartPosition`
- `observedActorPositionInitialized`

想定構造:

```cpp
struct GameLoopRuntime
{
    uint32_t currentNodeId = 0;
    uint32_t previousNodeId = 0;
    uint32_t pendingNodeId = 0;

    std::string currentScenePath;
    std::string pendingScenePath;

    bool sceneTransitionRequested = false;
    bool waitingSceneLoad = false;

    float nodeTimer = 0.0f;

    DirectX::XMFLOAT3 observedActorStartPosition{ 0.0f, 0.0f, 0.0f };
    bool observedActorPositionInitialized = false;
};
```

`ActorMovedDistance` 用に、node 開始時の actor 位置を保持する。

scene 遷移成功時には次を行う。

- `previousNodeId = currentNodeId`
- `currentNodeId = pendingNodeId`
- `currentScenePath = pendingScenePath`
- `pendingNodeId = 0`
- `pendingScenePath.clear()`
- `sceneTransitionRequested = false`
- `waitingSceneLoad = false`
- `nodeTimer = 0.0f`
- `observedActorPositionInitialized = false`

## 10. GameLoopSystem

`GameLoopSystem` は scene を直接 load しない。

役割:

- `GameLoopRuntime.currentNodeId` を見る
- current node から出る transition を列挙する
- transition condition を評価する
- 成立した transition があれば `pendingNodeId` / `pendingScenePath` を設定する
- `sceneTransitionRequested` を `true` にする

禁止:

- `PrefabSystem::LoadSceneIntoRegistry` を直接呼ばない
- `Registry` を直接 clear しない
- `EditorLayer` を直接操作しない
- scene load 成功後の current node 更新を自分で確定しない

処理手順:

1. Play 中でなければ何もしない
2. `runtime.waitingSceneLoad` が true なら何もしない
3. `runtime.sceneTransitionRequested` が true なら何もしない
4. `runtime.nodeTimer += dt`
5. `currentNodeId` に一致する node を asset から探す
6. `fromNodeId == currentNodeId` の transition を配列順に走査する
7. 各 transition の condition を評価する
8. 最初に成立した transition の `toNodeId` を pending に設定する
9. `runtime.sceneTransitionRequested = true` にする

疑似コード:

```cpp
void GameLoopSystem::Update(
    const GameLoopAsset& asset,
    GameLoopRuntime& runtime,
    Registry& registry,
    float dt)
{
    if (!IsPlaying()) return;
    if (runtime.waitingSceneLoad) return;
    if (runtime.sceneTransitionRequested) return;

    runtime.nodeTimer += dt;

    for (const auto& transition : asset.transitions) {
        if (transition.fromNodeId != runtime.currentNodeId) continue;
        if (!EvaluateTransition(transition, runtime, registry)) continue;

        const GameLoopNode* toNode = FindNode(asset, transition.toNodeId);
        if (!toNode) return;

        runtime.pendingNodeId = toNode->id;
        runtime.pendingScenePath = toNode->scenePath;
        runtime.sceneTransitionRequested = true;
        return;
    }
}
```

## 11. SceneTransitionSystem

`SceneTransitionSystem` は実際の scene load を担当する。

役割:

- `sceneTransitionRequested` を見る
- `pendingScenePath` を見る
- `EngineKernel::ResetRenderStateForSceneChange` 相当の処理を通す
- 既存の `PrefabSystem::LoadSceneIntoRegistry` 経路で scene を読む
- 成功時に `GameLoopRuntime.currentNodeId` / `currentScenePath` を更新する

scene load は frame 末尾で実行する。frame 途中で `Registry` を差し替えない。

処理手順:

1. frame 末尾の scene transition phase で実行する
2. `runtime.sceneTransitionRequested` が false なら何もしない
3. `runtime.pendingScenePath` が空なら error として request を破棄する
4. `runtime.waitingSceneLoad = true` にする
5. render state reset を実行する
6. 既存 scene load 経路で `pendingScenePath` を読む
7. 成功したら runtime の current / previous / pending を更新する
8. 失敗したら current scene を維持し、error を log / debug UI に出す

疑似コード:

```cpp
void SceneTransitionSystem::UpdateEndOfFrame(
    GameLoopRuntime& runtime,
    Registry& registry)
{
    if (!runtime.sceneTransitionRequested) return;
    if (runtime.pendingScenePath.empty()) {
        runtime.sceneTransitionRequested = false;
        return;
    }

    runtime.waitingSceneLoad = true;

    EngineKernel::Instance().ResetRenderStateForSceneChange();

    const bool ok = PrefabSystem::LoadSceneIntoRegistry(
        runtime.pendingScenePath,
        registry);

    if (!ok) {
        runtime.waitingSceneLoad = false;
        runtime.sceneTransitionRequested = false;
        return;
    }

    runtime.previousNodeId = runtime.currentNodeId;
    runtime.currentNodeId = runtime.pendingNodeId;
    runtime.currentScenePath = runtime.pendingScenePath;
    runtime.pendingNodeId = 0;
    runtime.pendingScenePath.clear();
    runtime.sceneTransitionRequested = false;
    runtime.waitingSceneLoad = false;
    runtime.nodeTimer = 0.0f;
    runtime.observedActorPositionInitialized = false;
}
```

## 12. GameLoopEditorPanel

`GameLoopEditorPanel` は GameLoopAsset を編集する UI である。

最初からノードグラフにしない。v0 はリスト式 UI とする。

構成:

- Toolbar
- Node List
- Node Inspector
- Transition List
- Transition Inspector
- Condition Editor
- Validate Result

Toolbar:

- `New Default Loop`
- `Load`
- `Save`
- `Save As`
- `Validate`

Node Inspector:

- `name`
- `scenePath`
- start node 指定

Transition Inspector:

- `fromNode`
- `toNode`
- `requireAllConditions`
- `conditions`

Condition Editor:

`InputPressed`:

- `actionIndex`

`TimerElapsed`:

- `seconds`

`ActorMovedDistance`:

- `actorType`
- `threshold`

`ActorDead`:

- `actorType`

`AllActorsDead`:

- `actorType`

EditorPanel は asset を編集するだけで、直接 scene transition を実行しない。Play 中の debug 表示として current node / pending node / node timer を表示してよいが、runtime state の変更は明示的な debug 操作に限定する。

## 13. Input 仕様

v0 は UI Button ではなく `InputActionMap` 経由で操作する。

GameLoop 用 action index:

| Index | Action |
| --- | --- |
| 0 | `Confirm` |
| 1 | `Cancel` |
| 2 | `Retry` |

Default binding:

| Action | Keyboard / Mouse | Gamepad |
| --- | --- | --- |
| `Confirm` | Enter / Space | Gamepad A |
| `Cancel` | Escape / Backspace | Gamepad B |
| `Retry` | R | Gamepad X |

`GameLoopSystem` は `InputEventQueue` を直接読まない。`InputResolveSystem` が更新した `ResolvedInputStateComponent` を読む。

GameLoop input を持つ entity は、scene 内または persistent input owner として次の component を持つ。

- `InputUserComponent`
- `InputBindingComponent`
- `InputActionMapComponent`
- `InputContextComponent`
- `ResolvedInputStateComponent`

`InputPressed` condition は `ResolvedInputStateComponent.actions[actionIndex].pressed` を見る。

## 14. ActorTypeComponent 連携

GameLoop の actor 判定には `ActorTypeComponent` を使う。

`PlayerTagComponent` は入力デバイス routing 用として残す。GameLoop の `ActorDead` / `AllActorsDead` / `ActorMovedDistance` では `PlayerTagComponent` ではなく `ActorTypeComponent` を使う。

対象 actor:

- `ActorType::Player`
- `ActorType::Enemy`
- `ActorType::NPC`
- `ActorType::Neutral`

Health を見る条件では、`ActorTypeComponent` + `HealthComponent` を持つ entity を対象とする。

Condition ごとの判定:

| Condition | 対象 component | 判定 |
| --- | --- | --- |
| `ActorDead` | `ActorTypeComponent` + `HealthComponent` | 指定 `actorType` のうち `health <= 0` の actor が 1 体以上いる |
| `AllActorsDead` | `ActorTypeComponent` + `HealthComponent` | 指定 `actorType` の actor が存在し、全員 `health <= 0` |
| `ActorMovedDistance` | `ActorTypeComponent` + `TransformComponent` | node 開始時からの移動距離が `threshold` 以上 |

`ActorMovedDistance` は v0 では指定 `actorType` の先頭 actor を観測対象にしてよい。将来、`targetName` や actor id 指定で対象を絞れるようにする。

## 15. Runtime 更新順

推奨更新順:

```text
InputContextSystem
InputResolveSystem
InputTextSystem

PlayerInputSystem
PlaybackSystem
StateMachineSystem
LocomotionSystem
CharacterPhysicsSystem
HealthSystem

GameLoopSystem
SceneTransitionSystem
```

理由:

- `InputPressed` を見るため、`InputResolveSystem` の後に `GameLoopSystem` を置く。
- `ActorMovedDistance` を見るため、`CharacterPhysicsSystem` の後に `GameLoopSystem` を置く。
- `ActorDead` / `AllActorsDead` を見るため、`HealthSystem` の後に `GameLoopSystem` を置く。
- scene load は frame 末尾で行うため、`SceneTransitionSystem` は最後に置く。

`SceneTransitionSystem` は `GameLoopSystem` の直後に呼ばれても、実際の load phase は frame 末尾扱いにする。render extraction や gameplay update の途中で `Registry` を差し替えない。

## 16. 標準 v0 ループ

まず以下を標準サンプルとする。

Node:

| Name | Scene Path |
| --- | --- |
| `Title` | `Data/Scenes/Title.scene` |
| `Battle` | `Data/Scenes/Battle.scene` |
| `Result` | `Data/Scenes/Result.scene` |

Transition:

| From | To | Condition |
| --- | --- | --- |
| `Title` | `Battle` | `InputPressed Confirm` |
| `Battle` | `Result` | `ActorMovedDistance Player threshold 1.0` |
| `Result` | `Battle` | `InputPressed Retry` |
| `Result` | `Title` | `InputPressed Cancel` |

`Battle -> Result` は勝敗ではなく、GameLoop の動作確認用に「Player が 1.0 unit 歩いたら遷移」とする。

想定 asset:

```text
startNodeId = Title

Title
  scenePath = Data/Scenes/Title.scene

Battle
  scenePath = Data/Scenes/Battle.scene

Result
  scenePath = Data/Scenes/Result.scene

Title -> Battle
  InputPressed actionIndex=0

Battle -> Result
  ActorMovedDistance actorType=Player threshold=1.0

Result -> Battle
  InputPressed actionIndex=2

Result -> Title
  InputPressed actionIndex=1
```

## 17. 実装フェーズ

### Phase 1: Runtime 固定ループ

EditorPanel なしで、コード上の固定 `GameLoopAsset` を作る。

目的:

- Title -> Battle -> Result -> Retry を runtime で通す
- scene load の責務分離を先に固める
- `GameLoopRuntime` が scene をまたいで保持されることを確認する

完成条件:

- Title scene で Confirm 入力を押すと Battle scene へ遷移する
- Battle scene で Player が 1.0 unit 歩くと Result scene へ遷移する
- Result scene で Retry 入力を押すと Battle scene を再ロードする

### Phase 2: GameLoopAsset 化

`.gameloop` ファイルを保存 / 読み込みできるようにする。

目的:

- `Data/GameLoop/Main.gameloop` を読む
- hard-coded loop を asset に置き換える
- serializer / deserializer を追加する

完成条件:

- 起動時または Play 開始時に `Data/GameLoop/Main.gameloop` を load できる
- load 失敗時は default loop を生成して warning を出す
- save / load 後も node id と transition が保たれる

### Phase 3: GameLoopEditorPanel

node / transition / condition を編集できる panel を作る。

目的:

- list UI で node と transition を編集する
- Validate できる
- Save / Load できる

完成条件:

- `New Default Loop` で v0 sample loop を作れる
- node name / scenePath を編集できる
- transition の from / to / condition を編集できる
- Validate result が UI に表示される
- `Data/GameLoop/Main.gameloop` として保存できる

### Phase 4: Condition 拡張

条件を増やす。

対象:

- `ActorDead`
- `AllActorsDead`
- `TimerElapsed`
- `RuntimeFlag`
- `ActorEnteredArea`
- `StateMachineState`
- `TimelineEvent`

完成条件:

- condition type ごとに validate と runtime evaluation が分離されている
- 未実装 condition は false 扱いにし、debug warning を出せる
- custom condition を追加しても既存 asset 形式を壊さない

## 18. Validate 仕様

`GameLoopEditorPanel` には Validate を必ず実装する。

確認項目:

- `startNodeId` が存在する
- node id が重複していない
- node name が空ではない
- `scenePath` が空ではない
- transition の `fromNodeId` が存在する
- transition の `toNodeId` が存在する
- condition が 0 件の transition がない
- `InputPressed` の `actionIndex` が有効範囲内
- `TimerElapsed` の `seconds` が 0 より大きい
- `ActorMovedDistance` の `threshold` が 0 より大きい
- Actor 系 condition の `actorType` が `None` ではない

Validate severity:

| Severity | 意味 |
| --- | --- |
| Error | Play / Save 前に修正が必要 |
| Warning | 動く可能性はあるが意図確認が必要 |
| Info | 補足情報 |

Error 例:

- `startNodeId` が存在しない
- `scenePath` が空
- transition の参照先 node が存在しない
- condition が 0 件

Warning 例:

- node が 1 つも transition を持たない
- unreachable node がある
- `RuntimeFlag` の `parameterName` が空

## 19. 受け入れ条件

v0 の受け入れ条件:

- Title scene から Confirm 入力で Battle scene へ遷移する
- Battle scene で Player が一定距離歩くと Result scene へ遷移する
- Result scene で Retry 入力を押すと Battle scene を再ロードする
- Result scene で Cancel 入力を押すと Title scene へ戻れる
- scene 遷移後も描画状態が壊れない
- GameLoop は Play 中のみ進行する
- `GameLoopRuntime` は scene load を跨いで保持される
- condition は勝敗専用ではなく、任意条件として追加できる構造になっている
- `GameLoopSystem` は scene load API を直接呼ばない
- `SceneTransitionSystem` は condition 評価を行わない
- scene load は frame 末尾で行われる

## 20. 禁止事項

禁止すること:

- 既存の Player gameplay runtime pipeline 仕様と混同すること
- `GameLoopSystem` から `PrefabSystem::LoadSceneIntoRegistry` を直接呼ぶこと
- `GameLoopSystem` から `Registry` を直接 clear すること
- `GameLoopSystem` から `EditorLayer` を直接操作すること
- scene load と次 scene 決定 logic を同じ system に混ぜること
- Title / Battle / Result を専用 C++ class として固定すること
- Battle 勝敗専用の hard-coded condition にすること
- `PlayerTagComponent` を GameLoop actor 判定の source of truth にすること
- `InputEventQueue` を `GameLoopSystem` が直接読むこと
- frame 途中で `Registry` を差し替えること
- v0 からノードグラフ UI を必須にすること

## 21. 将来拡張

将来拡張候補:

- `ActorEnteredArea`
- `StateMachineState`
- `TimelineEvent`
- `CustomEvent`
- async scene load
- fade in / fade out transition
- loading screen
- scene additive load
- persistent actor carry-over
- checkpoint / continue
- GameLoopRuntime の save data 化
- debug overlay で current node / pending node / condition result を表示
- node graph editor 化
- sub loop / nested loop
- DLC / chapter / stage select 用の複数 GameLoopAsset

将来拡張しても守るべき分離:

- GameLoop authoring data は `GameLoopAsset`
- scene を決めるのは `GameLoopSystem`
- scene を load するのは `SceneTransitionSystem`
- scene をまたいで残る状態は `GameLoopRuntime`
- actor 判定は `ActorTypeComponent`
- input 判定は `ResolvedInputStateComponent`
