# GameLoop Function Improvement Spec

作成日: 2026-05-02

対象:

- `Source/GameLoop/GameLoopAsset.h`
- `Source/GameLoop/GameLoopAsset.cpp`
- `Source/GameLoop/GameLoopRuntime.h`
- `Source/GameLoop/GameLoopRuntime.cpp`
- `Source/GameLoop/GameLoopSystem.h`
- `Source/GameLoop/GameLoopSystem.cpp`
- `Source/GameLoop/SceneTransitionSystem.h`
- `Source/GameLoop/SceneTransitionSystem.cpp`
- `Source/GameLoop/UIButtonClickEventQueue.h`
- `Source/GameLoop/UIButtonClickEventQueue.cpp`
- `Source/GameLoop/UIButtonClickSystem.h`
- `Source/GameLoop/UIButtonClickSystem.cpp`
- `Source/GameLoop/GameLoopEditorPanel*.{h,cpp}`
- `Source/Engine/EngineKernel.{h,cpp}`

関連データ:

- `Data/GameLoop/Main.gameloop`
- `Data/Scene/*.scene`

## 1. 目的

`Source/GameLoop` の現行実装は、GameLoop の基本的な asset / graph editor / scene transition / UI button click queue の骨格を持っている。一方で runtime の遷移判定は keyboard / gamepad の単発入力に寄っており、すでに runtime state に用意されている `nodeTimer`、`flags`、`observedActorStartPosition`、`UIButtonClickEventQueue`、`GameLoopLoadingPolicy` が十分に活用されていない。

この仕様では、GameLoop を「scene 単位の進行 graph」として実用できる状態へ改善する。具体的には、複数 condition による transition 評価、UI button click による遷移、timer / actor / runtime flag 条件、loading policy の反映、editor / validate の追従を実装対象として定義する。

## 2. 現状

### 2.1 できていること

- `.gameloop` JSON は `version = 4` として node / transition を保存できる。
- node は `id`、`name`、`scenePath`、`type`、`graphPos` を持つ。
- transition は `id`、`fromNodeId`、`toNodeId`、`name`、`GameLoopTransitionInput`、`GameLoopLoadingPolicy` を持つ。
- `ValidateGameLoopAsset` は node / transition の参照整合、scenePath、到達可能性、loading 秒数を検証できる。
- `GameLoopSystem` は現在 node の outgoing transition を配列順に評価し、入力が押されたら `runtime.pending*` を設定する。
- `SceneTransitionSystem` は frame 境界で `PrefabSystem::LoadSceneIntoRegistry` を呼び、成功時に runtime の current / pending を入れ替える。
- `UIButtonClickSystem` と `UIButtonClickEventQueue` は 2D UI button click を frame-local event として収集できる。
- `GameLoopEditorPanel` は graph 表示、node / transition 作成、scene picker、loading policy 編集、validate、save / load の土台を持つ。
- `EngineKernel` は `GameLoopAsset`、`GameLoopRuntime`、`m_gameLoopRegistry`、`UIButtonClickEventQueue` を保持し、Play / Stop と連動している。

### 2.2 改善が必要なこと

- `GameLoopSystem` は `clickQueue` と `gameLoopRegistry` をまだ遷移判定に使っていない。
- transition 条件が `GameLoopTransitionInput` 1 種類に固定されている。
- `GameLoopRuntime::nodeTimer` は加算されるが、`TimerElapsed` 条件が存在しない。
- `GameLoopRuntime::flags` は保存されるが、`RuntimeFlag` 条件が存在しない。
- `observedActorStartPosition` は reset されるが、actor 移動距離条件が存在しない。
- `GameLoopLoadingPolicy` は asset / editor に存在するが、scene transition の見た目や input block に反映されていない。
- `SceneTransitionSystem` は同一 scene への node 遷移と強制 reload の扱いを区別していない。
- `ValidateGameLoopAsset` は input 未設定の検証はできるが、複数 condition の型別検証ができない。
- editor は transition condition の詳細編集 UI を持たない。

## 3. 改善目標

GameLoop 改善の完了状態は次の通り。

- transition は複数 condition を持てる。
- transition は `requireAllConditions` により AND / OR 評価を切り替えられる。
- keyboard / gamepad 入力、UI button click、timer、actor death、actor moved distance、runtime flag で遷移できる。
- `transitions` 配列順は同一 `fromNodeId` 内の優先度として扱う。
- 同一 frame で成立する transition は最大 1 件だけ消費される。
- scene load は引き続き `SceneTransitionSystem` のみが実行する。
- `GameLoopSystem` は condition 評価と pending 設定だけを行う。
- `SceneTransitionSystem` は loading policy を参照し、最小限でも `Immediate` / `FadeOnly` / `LoadingOverlay` の状態遷移を runtime に反映する。
- editor から condition の追加 / 編集 / 削除 / 並び替えができる。
- validate で runtime 実行不能な asset を Play 前に検出できる。
- `version = 4` の既存 `.gameloop` は読み込める。保存時は `version = 5` として出力する。

## 4. 非目標

- async scene loading の完全実装はこの仕様の必須範囲に含めない。
- save data / checkpoint / chapter select は含めない。
- additive scene load は含めない。
- node graph editor の大規模 UI 改修は含めない。
- gameplay 専用の hard-coded Clear 判定は作らない。
- `GameLoopSystem` から `PrefabSystem::LoadSceneIntoRegistry` を直接呼ばない。
- `GameLoopRuntime` を scene registry の component として持たせない。

## 5. Data Schema v5

### 5.1 GameLoopConditionType

```cpp
enum class GameLoopConditionType : uint8_t
{
    None = 0,
    InputPressed,
    UIButtonClicked,
    TimerElapsed,
    ActorDead,
    AllActorsDead,
    ActorMovedDistance,
    RuntimeFlag,
};
```

### 5.2 GameLoopCondition

```cpp
struct GameLoopCondition
{
    GameLoopConditionType type = GameLoopConditionType::None;

    uint32_t keyboardScancode = 0;
    uint8_t gamepadButton = 0xFF;

    std::string targetName;
    std::string parameterName;

    ActorType actorType = ActorType::None;
    float seconds = 0.0f;
    float threshold = 0.0f;
};
```

field の用途:

| condition | 利用 field | 成立条件 |
|---|---|---|
| `None` | なし | 常に true |
| `InputPressed` | `keyboardScancode`, `gamepadButton` | 指定 key / button がこの frame に押された |
| `UIButtonClicked` | `targetName` | `clickQueue.Contains(targetName)` が true |
| `TimerElapsed` | `seconds` | `runtime.nodeTimer >= seconds` |
| `ActorDead` | `actorType` | 指定 actor type の dead entity が 1 体以上ある |
| `AllActorsDead` | `actorType` | 指定 actor type が 1 体以上存在し、全員 dead |
| `ActorMovedDistance` | `actorType`, `threshold` | node 開始時から指定 actor が `threshold` 以上移動 |
| `RuntimeFlag` | `parameterName` | `runtime.flags[parameterName] == true` |

### 5.3 GameLoopTransition

`GameLoopTransitionInput input` は v4 互換読み込み用として残し、v5 保存では `conditions` を正とする。

```cpp
struct GameLoopTransition
{
    uint32_t id = 0;
    uint32_t fromNodeId = 0;
    uint32_t toNodeId = 0;
    std::string name;

    std::vector<GameLoopCondition> conditions;
    bool requireAllConditions = true;

    GameLoopTransitionInput legacyInput;
    GameLoopLoadingPolicy loadingPolicy;
};
```

移行ルール:

- v4 読み込み時、`input.keyboardScancode != 0` または `input.gamepadButton != 0xFF` なら `InputPressed` condition 1 件へ変換する。
- v4 読み込み時、input が未設定なら `conditions` は空のまま読み、validate error とする。
- v5 保存時、`input` は書き出さない。
- v5 保存時、`conditions`、`requireAllConditions`、`loading` を書き出す。

### 5.4 JSON 例

```json
{
  "version": 5,
  "startNodeId": 1,
  "nextNodeId": 3,
  "nextTransitionId": 2,
  "nodes": [
    {
      "id": 1,
      "name": "Title",
      "scenePath": "Data/Scene/Title.scene",
      "type": "Scene",
      "posX": 0.0,
      "posY": 0.0
    },
    {
      "id": 2,
      "name": "Battle",
      "scenePath": "Data/Scene/Battle.scene",
      "type": "Scene",
      "posX": 360.0,
      "posY": 0.0
    }
  ],
  "transitions": [
    {
      "id": 1,
      "fromNodeId": 1,
      "toNodeId": 2,
      "name": "Start",
      "requireAllConditions": false,
      "conditions": [
        { "type": "UIButtonClicked", "targetName": "StartButton" },
        { "type": "InputPressed", "keyboardScancode": 40, "gamepadButton": 0 }
      ],
      "loading": {
        "mode": "FadeOnly",
        "fadeOutSeconds": 0.15,
        "fadeInSeconds": 0.15,
        "minimumLoadingSeconds": 0.0,
        "blockInput": true
      }
    }
  ]
}
```

## 6. Runtime 評価仕様

### 6.1 GameLoopSystem::Update

処理順:

1. `runtime.isActive == false` なら return。
2. `EngineMode::Play` でなければ return。
3. `runtime.waitingSceneLoad == true` なら return。
4. `runtime.sceneTransitionRequested == true` なら return。
5. `runtime.currentNodeId == 0` なら return。
6. `runtime.nodeTimer += dt`。
7. `ActorMovedDistance` が使われる可能性がある場合、観測 actor の開始位置を必要に応じて初期化する。
8. `asset.transitions` を配列順に走査する。
9. `transition.fromNodeId != runtime.currentNodeId` は skip。
10. `EvaluateTransition` が true になった最初の transition を採用する。
11. to node を解決し、`runtime.pendingNodeId`、`runtime.pendingScenePath`、`runtime.sceneTransitionRequested`、`runtime.forceReload` を設定して return。

### 6.2 EvaluateTransition

```cpp
bool EvaluateTransition(const GameLoopTransition& transition, ...)
{
    if (transition.conditions.empty()) {
        return false;
    }

    if (transition.requireAllConditions) {
        for (const auto& c : transition.conditions) {
            if (!EvaluateCondition(c, ...)) return false;
        }
        return true;
    }

    for (const auto& c : transition.conditions) {
        if (EvaluateCondition(c, ...)) return true;
    }
    return false;
}
```

### 6.3 InputPressed

`InputPressed` は現行の `InputEventQueue` 直接評価を継続する。`keyboardScancode` と `gamepadButton` のどちらかが指定されていれば成立候補とする。

要件:

- keyboard は `InputEventType::KeyDown` かつ `repeat == false` のみ true。
- gamepad は `InputEventType::GamepadButtonDown` のみ true。
- key / button の両方が設定されている場合は OR 評価。

### 6.4 UIButtonClicked

`UIButtonClicked` は `UIButtonClickEventQueue` を参照する。

要件:

- `targetName.empty()` は常に false。
- `clickQueue.Contains(targetName)` が true の frame だけ成立する。
- queue は `EngineKernel` が GameLoop 評価後に clear する。
- `UIButtonClickSystem` は game registry だけを見て click event を push する。

### 6.5 TimerElapsed

`TimerElapsed` は node 入場からの経過秒数で判定する。

要件:

- `seconds <= 0.0f` は validate error。
- `runtime.nodeTimer` は scene transition 成功時に 0 に戻す。
- 同一 scene 内で node だけが変わる場合も `runtime.nodeTimer` は 0 に戻す。

### 6.6 ActorDead / AllActorsDead

actor 系条件は `ActorTypeComponent` と `HealthComponent` を source of truth とする。

要件:

- `actorType == ActorType::None` は validate error。
- `ActorDead` は matching actor が 1 体以上 dead なら true。
- `AllActorsDead` は matching actor が 1 体以上存在し、全員 dead なら true。
- matching actor が 0 体の場合、`AllActorsDead` は false。

### 6.7 ActorMovedDistance

`ActorMovedDistance` は指定 actor type の先頭 entity を観測対象とする。

要件:

- `actorType == ActorType::None` は validate error。
- `threshold <= 0.0f` は validate error。
- 観測 actor が見つからない frame は false。
- 観測 actor が初めて見つかった frame で `observedActorStartPosition` を記録し、その frame は false。
- 距離は XZ 平面距離を使用する。
- scene / node 遷移成功時に `observedActorPositionInitialized = false` に戻す。

### 6.8 RuntimeFlag

`RuntimeFlag` は gameplay 側から `EngineKernel::Instance().GetGameLoopRuntime().flags[name] = true` で立てられることを想定する。

要件:

- `parameterName.empty()` は validate error。
- `runtime.flags.find(parameterName)` が存在し、値が true のとき成立する。
- flag は scene transition では消さない。
- Stop では `GameLoopRuntime::Reset()` により消す。

## 7. Scene Transition 仕様

### 7.1 同一 scene 遷移

`pendingScenePath == currentScenePath` の場合でも、node id が違うなら GameLoop 上の node 遷移として扱う。

要件:

- `forceReload == true` の場合は scene を再 load する。
- `forceReload == false` かつ same scene の場合は scene load を省略し、runtime の node 状態だけ更新する。
- node 状態だけ更新した場合も `nodeTimer` と `observedActorPositionInitialized` は reset する。

### 7.2 LoadingPolicy 適用

`GameLoopLoadingPolicy` は `SceneTransitionSystem` が直接参照できるようにする。方法は次のどちらかに統一する。

- `GameLoopRuntime` に `pendingLoadingPolicy` を持たせる。
- `SceneTransitionSystem::UpdateEndOfFrame` に `const GameLoopAsset& asset` を渡し、`pendingTransitionId` から解決する。

この仕様では、`GameLoopSystem` が選んだ transition を明示できるように `GameLoopRuntime::pendingTransitionId` と `pendingLoadingPolicy` を追加する。

```cpp
uint32_t pendingTransitionId = 0;
GameLoopLoadingPolicy pendingLoadingPolicy;
float loadingElapsedSeconds = 0.0f;
```

Phase 1 では `Immediate` のみ scene load に反映してよい。ただし `pendingLoadingPolicy` は runtime に保存し、debug 表示と validate 対象にする。

Phase 2 で `FadeOnly` と `LoadingOverlay` を反映する。

### 7.3 Immediate

- input block は行わない。
- `SceneTransitionSystem` 呼び出し frame で同期 load を実行する。
- 成功 / 失敗時の runtime 更新は現行処理を継続する。

### 7.4 FadeOnly

- fade out 完了後に scene load し、load 成功後に fade in する。
- fade 中は `runtime.waitingSceneLoad = true` として GameLoop 評価を止める。
- `blockInput == true` の場合、UI button click queue と gameplay input は遷移に使われない。
- fade 描画の実体がまだない場合、Phase 2 の最小実装では debug overlay state だけを持つ。

### 7.5 LoadingOverlay

- `minimumLoadingSeconds` が経過するまで scene load 完了を表示上隠す。
- `loadingMessage` が空でなければ overlay に表示する。
- async loading がない間は、同期 load 後に `minimumLoadingSeconds` を満たすまで current scene 表示を維持するか、専用 overlay を表示する。

## 8. Editor 改善仕様

### 8.1 Transition Inspector

transition 選択時に次を編集できる。

- `name`
- `fromNodeId`
- `toNodeId`
- `requireAllConditions`
- `conditions`
- `loadingPolicy`

condition list 操作:

- Add Condition
- Duplicate Condition
- Delete Condition
- Move Up
- Move Down

### 8.2 Condition Editor

condition type ごとの UI:

| type | UI |
|---|---|
| `None` | 追加 field なし |
| `InputPressed` | keyboard scancode int、gamepad button int、Capture Next Input |
| `UIButtonClicked` | targetName text、scene 内 button id 候補 list |
| `TimerElapsed` | seconds float |
| `ActorDead` | actorType combo |
| `AllActorsDead` | actorType combo |
| `ActorMovedDistance` | actorType combo、threshold float |
| `RuntimeFlag` | parameterName text |

`Capture Next Input` は Phase 2 でよい。Phase 1 では int 入力だけでよい。

### 8.3 Graph 表示

transition label は condition summary を表示する。

例:

```text
Start [Input/UIButton]
Clear [AllDead Enemy]
Retry [Timer 3.0s]
```

要件:

- condition が 0 件なら `[No Condition]` を error color で表示する。
- condition が 3 件以上ある場合は先頭 2 件 + `+N` で省略する。
- selected transition は condition count と loading mode を見えるようにする。

### 8.4 Validate 表示

validate message は可能なら対象 object id を持つ。

```cpp
struct GameLoopValidateMessage
{
    GameLoopValidateSeverity severity;
    std::string message;
    uint32_t nodeId = 0;
    uint32_t transitionId = 0;
    int conditionIndex = -1;
};
```

要件:

- message click で対象 node / transition / condition を選択する。
- Graph 上の node / transition に error / warning badge を表示する。
- Save と Play 前には validate を実行する。
- Error がある場合、Play は GameLoop 起動を中止する。

## 9. Validate 仕様

### 9.1 Asset

Error:

- `version` が対応外。
- `nodes` が空。
- `startNodeId` が存在しない。
- node id が重複。
- transition id が 0。
- transition id が重複。

Warning:

- 到達不能 node。
- outgoing transition がない node。

### 9.2 Node

Error:

- `scenePath` が空。
- `scenePath` が absolute path。
- `scenePath` が `../` を含む。
- `scenePath` が `Data/` 配下に正規化できない。

Warning:

- scene file が存在しない。
- `graphPos` が finite でない。
- `graphPos` が origin から極端に遠い。

### 9.3 Transition

Error:

- `fromNodeId` が存在しない。
- `toNodeId` が存在しない。
- `conditions` が空。
- `loadingPolicy.fadeOutSeconds < 0.0f`。
- `loadingPolicy.fadeInSeconds < 0.0f`。
- `loadingPolicy.minimumLoadingSeconds < 0.0f`。

Warning:

- 同じ `fromNodeId` / `toNodeId` / condition summary の transition が重複している。
- `name` が空。

### 9.4 Condition

Error:

- `InputPressed` で keyboard / gamepad の両方が未設定。
- `UIButtonClicked` で `targetName` が空。
- `TimerElapsed` で `seconds <= 0.0f`。
- actor 系 condition で `actorType == ActorType::None`。
- `ActorMovedDistance` で `threshold <= 0.0f`。
- `RuntimeFlag` で `parameterName` が空。

Warning:

- `UIButtonClicked.targetName` と一致する button id が現在開いている scene に見つからない。
- `TimerElapsed.seconds` が極端に短い。目安は `< 0.05f`。

## 10. EngineKernel 連携

### 10.1 Update 順

GameLoop 関連は次の順で実行する。

```text
PollInput
GameLayer::Update
InputResolveSystem::Update(m_gameLoopRegistry)
UIButtonClickSystem::Update(gameRegistry)
GameLoopSystem::Update
SceneTransitionSystem::UpdateEndOfFrame
UIButtonClickEventQueue::Clear
Render
```

現在は `SceneTransitionSystem::UpdateEndOfFrame` が `GameLayer::Update` より前に呼ばれる箇所がある。最終形では GameLayer 内の click / gameplay / physics 結果を同 frame で拾えるように、GameLayer update 後、Render 前に scene transition を実行する。

実装時は描画 resource の無効化を避けるため、Render 開始後には scene load しない。

### 10.2 Play

Play 開始時:

- `ValidateGameLoopAsset` を実行する。
- Error があれば `runtime.isActive = false` のままにして Play を中止、または GameLoop 無効で通常 Play にする。Editor 操作としては中止が推奨。
- `runtime.Reset()`。
- `runtime.isActive = true`。
- start node を pending に設定する。
- `runtime.forceReload = true`。
- `runtime.pendingTransitionId = 0`。
- `runtime.pendingLoadingPolicy = Immediate`。

### 10.3 Stop

Stop 時:

- `runtime.Reset()`。
- `UIButtonClickEventQueue::Clear()`。
- Play 前の editor scene を復元する。

## 11. 実装フェーズ

### Phase 0: 互換土台

目的: v4 asset を壊さず、v5 schema を追加できる状態にする。

作業:

- `GameLoopConditionType` と `GameLoopCondition` を追加する。
- `GameLoopTransition` に `conditions`、`requireAllConditions`、`legacyInput` を追加する。
- v4 load 時に legacy input を `InputPressed` condition へ変換する。
- save は v5 で出力する。
- validate を condition 対応へ拡張する。

完了条件:

- 既存 `Data/GameLoop/Main.gameloop` を読み込める。
- 保存後に `version = 5` になる。
- v4 input transition は v5 `InputPressed` として動く。

### Phase 1: Runtime condition 評価

目的: 複数 condition により scene 遷移できる。

作業:

- `EvaluateTransition` と `EvaluateCondition` を実装する。
- `UIButtonClicked` を `clickQueue` と接続する。
- `TimerElapsed` を `nodeTimer` と接続する。
- actor 系 condition を `ActorTypeComponent` / `HealthComponent` / `TransformComponent` と接続する。
- `RuntimeFlag` を `runtime.flags` と接続する。
- `pendingTransitionId` と `pendingLoadingPolicy` を runtime に追加する。

完了条件:

- keyboard / gamepad input で遷移できる。
- UI button click で遷移できる。
- timer で遷移できる。
- actor death または all dead で遷移できる。
- runtime flag で遷移できる。
- 1 frame で複数遷移しない。

### Phase 2: Scene transition と loading policy

目的: transition ごとの loading policy を runtime に反映する。

作業:

- same scene + `forceReload == false` の場合に scene load を省略する。
- `pendingLoadingPolicy` を `SceneTransitionSystem` が消費する。
- `Immediate` を現行同期 load として維持する。
- `FadeOnly` と `LoadingOverlay` の runtime state を追加する。
- input block 中は GameLoop 評価を止める。

完了条件:

- self-loop で scene reload する / しないを制御できる。
- loading policy が debug 表示で確認できる。
- `minimumLoadingSeconds` が runtime state として守られる。

### Phase 3: Editor condition UI

目的: editor から v5 asset を authoring できる。

作業:

- transition inspector に condition list を追加する。
- condition type ごとの editor を追加する。
- graph label に condition summary を出す。
- validate badge と message click selection を追加する。
- Save / Play 前 validate を強化する。

完了条件:

- editor だけで Title -> Battle -> Result -> Retry の loop を作れる。
- condition を追加 / 削除 / 並び替えできる。
- invalid condition が graph と validate panel に表示される。

### Phase 4: Usability polish

目的: 実制作で詰まりにくくする。

作業:

- button id 候補 picker。
- next input capture。
- condition preset。
- transition priority move。
- runtime debug panel。
- current node / pending transition highlight。

完了条件:

- condition 名や button id を手入力だけに頼らず設定できる。
- Play 中に current node と pending transition が editor で追える。

## 12. 受け入れ条件

- `Debug x64` build が通る。
- 既存 v4 `.gameloop` を load して v5 へ save できる。
- Play 開始時に start node scene が load される。
- keyboard / gamepad input condition で scene 遷移できる。
- UI button click condition で scene 遷移できる。
- `TimerElapsed` condition で scene 遷移できる。
- `AllActorsDead` condition で scene 遷移できる。
- `RuntimeFlag` condition で scene 遷移できる。
- condition が空の transition は validate error になる。
- actor 系 condition の `ActorType::None` は validate error になる。
- invalid asset では Play 開始時に GameLoop を起動しない。
- same scene transition で reload する / しないを制御できる。
- `SceneTransitionSystem` 以外から scene load しない。
- `GameLoopSystem` は `UIButtonClickEventQueue` を書き換えない。
- Stop で `GameLoopRuntime` と click queue が reset される。

## 13. 禁止事項

- `GameLoopSystem` に scene load 処理を入れない。
- `SceneTransitionSystem` に condition 評価を入れない。
- transition priority を map / unordered_map の iteration order に依存させない。
- `UIButtonClickEventQueue` を frame を跨いで残さない。
- `ActorTypeComponent` ではなく name 文字列だけで actor 系 condition を判定しない。
- v4 asset を読み込み不能にしない。
- loading policy のために Render 中に registry を差し替えない。

## 14. 最初に直す順序

1. `GameLoopAsset` を v5 schema 対応にする。
2. v4 input を v5 `InputPressed` に変換する。
3. `ValidateGameLoopAsset` を condition 対応にする。
4. `GameLoopSystem` に `EvaluateTransition` / `EvaluateCondition` を追加する。
5. `UIButtonClicked` と `TimerElapsed` を先に通す。
6. actor 系 condition と `RuntimeFlag` を追加する。
7. `SceneTransitionSystem` に same scene / force reload の分岐を入れる。
8. editor inspector に condition 編集 UI を追加する。
9. graph label と validate badge を condition 対応にする。
10. loading policy の表示 / runtime state を実装する。
