# GameFlow System Improvement Spec

作成日: 2026-05-04
対象: `Source/GameLoop`, `GameLoopEditorPanel`, `GameLoopSystem`, `SceneTransitionSystem`, `UIButtonClickEventQueue`, `BattleFlowSystem`

---

## 1. 結論

`GameLoop` は `GameFlow` に改名し、Scene遷移専用ツールではなく、UE Blueprintに近い「ゲーム進行をイベントとアクションで組むFlow Graph Editor」として育てる。

ただし、`UIButton`、`BattleFlow`、`SceneTransition` の責務は混ぜない。

- `UIButton`: クリックイベントを発行するだけ。
- `BattleFlow`: 戦闘中の専門状態を進め、結果イベントを発行する。
- `GameFlow`: UI、入力、BattleFlow、Scene、Scriptから来たイベントを受け、Scene遷移、状態遷移、Fade、Flag操作、Battle開始などを実行する。

初回実装では、巨大な汎用Blueprintを作り切るのではなく、GameFlow新形式へ素直に切り替え、`FlowEventQueue` と `Transition Action` を追加する。

---

## 2. 現状整理

### 2.1 GameLoop

現在の構造:

- `GameLoopAsset`
  - `version = 4`
  - `nodes`
  - `transitions`
  - `GameLoopNodeType::Scene` のみ
- `GameLoopTransitionInput`
  - `keyboardScancode`
  - `gamepadButton`
  - `uiButtonId`
- `GameLoopSystem`
  - Play中に現在Nodeから出るTransitionを走査
  - keyboard / gamepad / UI Button入力があれば `pendingScenePath` を設定
- `SceneTransitionSystem`
  - `PrefabSystem::LoadSceneIntoRegistry()` で同期Scene load
- `GameLoopEditorPanel`
  - Scene Node graph
  - Transition線
  - Transition Inspector
  - Loading Policy editor

弱い点:

- 名前が `GameLoop` のままで、実際に作りたい役割は `GameFlow`。
- Node typeがSceneだけで、状態Node、Event Node、Action Nodeがない。
- Transition条件が入力直結で、汎用イベントではない。
- `GameLoopRuntime::flags` が存在するが、実際の条件評価やActionから使われていない。
- `GameLoopLoadingPolicy` は保存されるが、SceneTransitionSystem側でFade/Overlayをまだ実行していない。
- `m_gameLoopRegistry` にInput ownerを作っているが、GameLoopSystemは `ResolvedInputStateComponent` より生のInputEventQueueを見ている。

### 2.2 BattleFlow

現在の構造:

- `BattleFlowComponent`
  - `phase`: Idle / Encounter / Combat / Victory / Defeat
  - `phaseTimer`
  - `playerEntity`
  - `bossEntity`
  - `arenaEntity`
  - `encounterRadius`
  - `introDuration`
- `BattleFlowSystem`
  - Player / Boss / Arenaを自動検索
  - IdleからEncounterへ
  - EncounterからCombatへ
  - Boss死亡でVictory
  - Player死亡でDefeat

弱い点:

- `_BattleFlow` は `GameLayer::Initialize()` で通常のgame registryに作られるが、Scene遷移時の `ClearRegistryForSceneReplace()` で保護対象ではない。Scene load後に消える可能性がある。
- `BattleFlowSystem` はPhase変更を外部へイベント発行しない。
- Victory/Defeat後の戻り先、リザルト表示、Scene遷移はコメント上「upstream UI / GameLoop」扱いだが、実際の接続点がない。
- `AutoBindEntities()` は便利だが、GameFlowから明示的に `StartBattle` する設計とは責務が少し衝突する。

---

## 3. 目標

### 3.1 最優先目標

1. `GameLoop Editor` をUI表示上 `GameFlow Editor` に改名する。
2. 既存 `.gameloop` 互換は不要。新しい `.gameflow` 形式を正とする。
3. `FlowEventQueue` を追加し、UI Button、Input、Scene、BattleFlow、Scriptイベントを同じ形式で扱う。
4. Transition条件を `GameLoopTransitionInput` 直結から `FlowCondition` へ拡張する。
5. Transition成立後に `LoadScene` だけでなく、複数の `FlowAction` を実行できるようにする。
6. BattleFlowはGameFlowへイベントを返し、GameFlowが次のSceneやUIへ進める。

### 3.2 非目標

初回実装では以下を必須にしない。

- UE Blueprint級の任意ノード実行環境。
- C++関数の自由呼び出し。
- Visual scripting VM。
- ループ、配列、変数スコープ、ローカル変数。
- async Scene streamingの完全実装。
- Battle system全体の作り直し。
- PlayerEditor / EffectEditor / MaterialEditorの改名や統合。

---

## 4. 命名と移行

### 4.1 表示名

Phase 1では、まずEditor上の表示名を変える。

- Window title: `GameLoop Editor` -> `GameFlow Editor`
- Toolbar / menu label: `GameLoop` -> `GameFlow`
- default path表示: `Data/GameFlow/Main.gameflow`

### 4.2 ファイル/クラス名

互換性を気にしなくてよいので、実装時点で新名称へ寄せる。

- C++ class/file nameは `GameFlow*` へ変更する。
- user-visible textも `GameFlow` にする。
- Save/Load pathは `Data/GameFlow/Main.gameflow` を正とする。
- `Data/GameLoop/Main.gameloop` のfallback読み込みは不要。
- 旧 `GameLoop*` compatibility headerも不要。

### 4.3 asset version

新形式は `version = 1` とする。

旧 `.gameloop` の読み込み変換は不要。

理由:

- 現時点でGameLoop assetは実運用していない。
- 互換処理を残すと、条件・Action・Editor実装が二重化する。
- GameFlow初期実装の品質を優先する。

---

## 5. GameFlowの責務

GameFlowは「ゲーム全体の進行」を扱う。

扱うもの:

- Title -> Gameplay -> Result のようなScene遷移。
- MainMenu / Pause / Option / BattleResult のような状態遷移。
- FadeOut / FadeIn / Loading overlay。
- `UIButton` やInput Actionから発生したイベント。
- BattleFlowから返る勝敗イベント。
- ScriptやC++側から投げるカスタムイベント。

扱わないもの:

- Buttonの見た目、hover、pressed状態。
- Battle内の1ターン処理、攻撃計算、AI判断。
- PlayerのAnimation State Machine。
- Effect editorのノードグラフ。

---

## 6. FlowEventQueue仕様

### 6.1 方針

イベント種類をC++ enumで増やしすぎず、`type` と `name` の二段構えにする。

```cpp
enum class FlowEventType : uint8_t
{
    UI,
    Input,
    Scene,
    Flow,
    Battle,
    Script,
    Timer,
    Error
};

struct FlowEvent
{
    FlowEventType type = FlowEventType::Flow;
    std::string name;
    std::string stringValue;
    EntityID entity = Entity::NULL_ID;
    float numberValue = 0.0f;
    uint32_t frame = 0;
};
```

`name` は `"ui.button.clicked"` のようなnamespaced stringにする。

### 6.2 初回実装で拾うイベント数

初回実装でEditorに組み込み候補として表示するイベントは18個とする。

この18個は「最初から全部のシステムを作る」という意味ではなく、GameFlowのイベント語彙として予約する数である。Phase 1の実装必須は、このうち12個。

| No | Event name | Type | Phase | Payload |
| --- | --- | --- | --- | --- |
| 1 | `ui.button.clicked` | UI | 1 | `stringValue = buttonId` |
| 2 | `input.action.pressed` | Input | 1 | `stringValue = actionName` |
| 3 | `input.action.released` | Input | 2 | `stringValue = actionName` |
| 4 | `flow.started` | Flow | 1 | current node |
| 5 | `flow.stopped` | Flow | 1 | none |
| 6 | `flow.node.entered` | Flow | 1 | node id/name |
| 7 | `flow.node.exited` | Flow | 2 | node id/name |
| 8 | `flow.transition.started` | Flow | 1 | transition id/name |
| 9 | `flow.transition.completed` | Flow | 2 | transition id/name |
| 10 | `flow.timer.elapsed` | Timer | 1 | timer name or seconds |
| 11 | `flow.flag.changed` | Flow | 1 | flag name |
| 12 | `flow.custom` | Script | 1 | custom event name |
| 13 | `scene.load.requested` | Scene | 2 | scene path |
| 14 | `scene.loaded` | Scene | 1 | scene path |
| 15 | `scene.load.failed` | Error | 1 | scene path |
| 16 | `battle.encounter.started` | Battle | 1 | battle id/entity |
| 17 | `battle.combat.started` | Battle | 2 | battle id/entity |
| 18 | `battle.ended` | Battle | 1 | `stringValue = Victory/Defeat` |

Phase 1必須イベント:

- `ui.button.clicked`
- `input.action.pressed`
- `flow.started`
- `flow.stopped`
- `flow.node.entered`
- `flow.transition.started`
- `flow.timer.elapsed`
- `flow.flag.changed`
- `flow.custom`
- `scene.loaded`
- `scene.load.failed`
- `battle.ended`

BattleFlowの詳細イベントはPhase 2でもよいが、`battle.ended` だけはPhase 1で拾う。

### 6.3 イベント順序

1 frame内では以下の順にGameFlowへ入る。

1. Input events
2. UI events
3. Gameplay/Battle events
4. Scene events
5. Script events
6. Timer events

同じframeに複数イベントがある場合は、`frame` とpush順を保持する。

### 6.4 消費ルール

初回実装では、イベントは原則broadcastでよい。

- 1つのeventを複数Transitionが見てもよい。
- 同じNodeから複数Transitionが同時成立した場合は、Graph上の優先順位で1つだけ選ぶ。
- Transitionに `priority` を追加する。
- 同priorityなら配列順、またはEditor上の表示順を使う。

---

## 7. FlowCondition仕様

既存の `GameLoopTransitionInput` を、複数条件を持てる構造に移行する。

```cpp
enum class FlowConditionType : uint8_t
{
    EventName,
    InputAction,
    UIButton,
    TimerElapsed,
    FlagEquals,
    BattleResult,
    SceneLoaded
};

struct FlowCondition
{
    FlowConditionType type = FlowConditionType::EventName;
    std::string name;
    std::string value;
    float numberValue = 0.0f;
    bool invert = false;
};
```

Transitionは `conditions` を持つ。

```cpp
struct GameFlowTransition
{
    uint32_t id = 0;
    uint32_t fromNodeId = 0;
    uint32_t toNodeId = 0;
    std::string name;
    int priority = 0;
    std::vector<FlowCondition> conditions;
    std::vector<FlowAction> actions;
    GameFlowConditionMode conditionMode = GameFlowConditionMode::All;
};
```

MVPでは `conditionMode = All` のみでもよい。

---

## 8. FlowAction仕様

Transition成立後に実行するActionを持つ。

```cpp
enum class FlowActionType : uint8_t
{
    None,
    LoadScene,
    SetCurrentNode,
    EmitEvent,
    SetFlag,
    ClearFlag,
    Fade,
    Wait,
    StartBattleFlow,
    ResetBattleFlow
};

struct FlowAction
{
    FlowActionType type = FlowActionType::None;
    std::string target;
    std::string value;
    float duration = 0.0f;
};
```

Phase 1必須Action:

- `LoadScene`
- `SetCurrentNode`
- `EmitEvent`
- `SetFlag`
- `ClearFlag`
- `StartBattleFlow`
- `ResetBattleFlow`

Phase 2必須Action:

- `Fade`
- `Wait`
- `ShowLoadingOverlay`
- `HideLoadingOverlay`

`GameLoopLoadingPolicy` は廃止し、Loading関連は `FlowAction` の列として表現する。

### 8.1 Loading画面の扱い

Loading画面は放置しない。ただしPhase 1の実装必須からは外す。

理由:

- 現在のScene loadは `PrefabSystem::LoadSceneIntoRegistry()` による同期loadであり、実際のload時間中に描画を進められない。
- この状態でLoading画面だけ先に実装しても、通常は一瞬で消えるか、`minimumLoadingSeconds` で人工的に待たせるだけになる。
- Fade / Overlay / async load / minimum display timeは、`FlowAction` 実行基盤ができてから接続した方が責務がきれい。

Phase 1では以下だけを守る。

- `FlowActionType::Fade`, `Wait`, `ShowLoadingOverlay`, `HideLoadingOverlay` を仕様上予約する。
- Transition Inspector上ではLoading PolicyではなくAction listとして表示する。

Phase 2で実装する。

- `Fade` action
- `ShowLoadingOverlay` / `HideLoadingOverlay` action
- `minimumLoadingSeconds`
- Scene load前後の `scene.load.requested`, `scene.loaded`, `scene.load.failed` event

Phase 3以降で検討する。

- async scene loading
- loading progress
- streaming / additive scene loading

例:

```text
Transition: StartGame
Conditions:
  ui.button.clicked == "StartGame"
Actions:
  Fade duration=0.15
  LoadScene target="Data/Scene/test.scene"
  SetCurrentNode target="Gameplay"
```

---

## 9. GameFlow Editor仕様

### 9.1 Panel構成

`GameFlow Editor` は以下の構成にする。

- Toolbar
  - New
  - Load
  - Save
  - Save As
  - Validate
  - Play Debug
- Graph
  - Scene Node
  - State Node
  - Battle Node
  - Event Node
- Inspector
  - Node Inspector
  - Transition Inspector
  - Condition list
  - Action list
- Event Catalog
  - built-in event 18個
  - custom event
- Validation panel
  - errors
  - warnings
- Runtime debug panel
  - current node
  - recent events
  - flags
  - pending transition

### 9.2 Node types

Phase 1:

- `Scene`
- `State`
- `Battle`

Phase 2:

- `Event`
- `Branch`
- `SubFlow`

`Scene` Nodeは旧 `GameLoopNode` の役割を引き継ぐが、保存形式の互換は持たない。

`State` NodeはScene pathを持たず、Menu状態やResult状態を表す。

`Battle` NodeはBattleFlow開始Actionを持つ補助Nodeとして扱う。

### 9.3 Transition Inspector

Transition Inspectorは以下を表示する。

- Name
- Priority
- From / To
- Conditions
- Actions
- Loading Policy migration preview

禁止:

- UIButton専用の遷移パネルにすること。
- Button Inspectorから遷移先Nodeを直接編集させること。

許可:

- Button側の `Event Name` をコピーし、Transition条件の `ui.button.clicked` に貼る。
- Event Catalogから `ui.button.clicked` 条件を追加する。

---

## 10. BattleFlow連携仕様

### 10.1 責務

BattleFlowは「戦闘の中」を扱う。

扱うもの:

- Encounter開始
- Combat開始
- 勝敗判定
- Battle phase timer
- Player/Boss/Arena参照

扱わないもの:

- Titleへ戻るScene遷移
- Result Sceneへの遷移
- Main Menu UI操作
- Loading / Fade

これらはGameFlowが扱う。

### 10.2 BattleFlowの永続性

現状の `_BattleFlow` は通常Scene置換で消える可能性がある。

採用方針: runtime singleton方式

- `BattleFlowRuntime` を `EngineKernel` または専用Serviceが持つ。
- Scene内の `BattleFlowComponent` はauthoring設定として扱う。
- Scene load後に `BattleFlowRuntimeSetup` がauthoring componentからruntimeへ反映する。
- GameFlowはruntimeへ `StartBattleFlow` / `ResetBattleFlow` を送る。

不採用案: persistent entity方式

- `ScenePersistentComponent` を追加。
- `ClearRegistryForSceneReplace()` がpersistent entityを破壊しない。
- `_BattleFlow` に `ScenePersistentComponent` を付ける。

不採用理由:

- Scene registry上のEntityを永続化すると、Scene authoring dataとruntime stateが混ざる。
- Scene置換時に保護対象が増え、Prefab/Scene save/loadの責務が曖昧になる。
- GameFlowからBattleFlowへ命令する境界が見えにくくなる。

よって、BattleFlowは `BattleFlowRuntime` を正とし、Scene内の `BattleFlowComponent` は設定ソースとして扱う。

```cpp
struct BattleFlowRuntime
{
    enum class Phase : uint8_t
    {
        Idle,
        Encounter,
        Combat,
        Victory,
        Defeat
    };

    std::string battleId;
    Phase phase = Phase::Idle;
    Phase previousPhase = Phase::Idle;
    float phaseTimer = 0.0f;
    EntityID playerEntity = Entity::NULL_ID;
    EntityID bossEntity = Entity::NULL_ID;
    EntityID arenaEntity = Entity::NULL_ID;
    bool active = false;
    bool resultEventEmitted = false;
};
```

`BattleFlowComponent` は以下のようなauthoring configへ寄せる。

```cpp
struct BattleFlowComponent
{
    std::string battleId;
    EntityID playerEntity = Entity::NULL_ID;
    EntityID bossEntity = Entity::NULL_ID;
    EntityID arenaEntity = Entity::NULL_ID;
    float encounterRadius = 18.0f;
    float introDuration = 1.5f;
    bool autoStartOnPlayerEnter = true;
};
```

### 10.3 BattleFlowが発行するイベント

BattleFlowはphaseが変わった瞬間だけGameFlowへイベントを送る。

必須:

- `battle.ended`
  - `stringValue = "Victory"` or `"Defeat"`

Phase 2:

- `battle.encounter.started`
- `battle.combat.started`
- `battle.phase.changed`

Event発行は毎frameではなく、phase transition時のみ。

### 10.4 BattleFlow改善候補

現在の `AutoBindEntities()` はfallbackとして残すが、主経路にしない。

改善:

- `BattleFlowComponent` に `battleId` を追加。
- `playerEntity`, `bossEntity`, `arenaEntity` をInspectorで明示設定できるようにする。
- 未設定時だけTag検索fallbackを使う。
- `Phase::Victory` / `Phase::Defeat` に入ったframeで `battle.ended` を一度だけ発行する。
- `ResetBattleFlow` Actionで `Idle` へ戻せるようにする。

---

## 11. Runtime更新順

現状の更新順は大筋維持してよい。

推奨順:

1. Input収集
2. EditorLayer Update
3. SceneTransitionSystem pending処理
4. GameLayer Update
5. UIButtonClickSystem
6. FlowEventCollector
7. GameFlowSystem
8. FlowActionExecutor
9. clear per-frame queues

理由:

- BattleFlowはGameLayer内で更新されるので、GameFlowは同frameでBattleFlow結果イベントを受け取れる。
- Scene loadは安全のためframe境界で実行する。
- UI Button clickはGame View cameraが必要なので、GameLayer update後に行う。

---

## 12. Data形式

### 12.1 新形式

拡張子:

- `.gameflow`

path:

- `Data/GameFlow/Main.gameflow`

version:

- `1`

概略:

```json
{
  "version": 5,
  "startNodeId": 1,
  "nodes": [],
  "transitions": [],
  "variables": [],
  "editor": {
    "graphZoom": 1.0
  }
}
```

### 12.2 旧形式の扱い

旧 `.gameloop` からの自動変換は実装しない。

必要になった場合のみ、別途one-shot converterを作る。

---

## 13. Validation

### 13.1 Error

- start nodeが存在しない。
- node id重複。
- transition id重複。
- transitionのfrom/toが存在しない。
- transitionにconditionがない。
- transitionにactionがない。
- `LoadScene` actionのscene pathが空。
- `LoadScene` actionのscene pathが `Data/` 外。
- `Battle` Nodeに `StartBattleFlow` actionがない。
- `battle.ended` 条件があるのにBattleFlow event collectorが無効。

### 13.2 Warning

- nodeにoutgoing transitionがない。
- nodeがstartから到達不能。
- custom event名が空。
- UI Button Event条件があるが、現在のSceneに同名Buttonが見つからない。
- Loading系Actionが未設定のTransitionで、ユーザーがLoading表示を期待している場合。
- BattleFlowComponentのEntity参照が未設定で、Tag fallbackに依存している。

---

## 14. 実装Phase

### Phase 0: 仕様反映と安全Rename

- Window表示名を `GameFlow Editor` に変更。
- DocsとUI上の説明をGameFlowへ変更。
- C++名を `GameFlow` へ変更する。
- `Data/GameFlow/Main.gameflow` を正規pathにする。
- 旧 `GameLoop` fallbackは実装しない。

受け入れ条件:

- Editor表示上はGameFlowと表示される。
- 新規 `Data/GameFlow/Main.gameflow` が保存/読み込みできる。

### Phase 1: FlowEventQueue MVP

- `FlowEvent` / `FlowEventQueue` を追加。
- `UIButtonClickEventQueue` を直接GameFlowが読むのではなく、`ui.button.clicked` eventへ変換する。
- `input.action.pressed` をInput ownerのResolvedInputから生成する。
- `battle.ended` をBattleFlowから生成する。
- `flow.timer.elapsed` / `flow.flag.changed` / `flow.custom` をRuntimeから扱えるようにする。

受け入れ条件:

- UI Button clickで `ui.button.clicked` が発生する。
- Keyboard/Gamepad Actionで `input.action.pressed` が発生する。
- BattleFlow Victory/Defeatで `battle.ended` が1回だけ発生する。
- Runtime debug panelに直近イベントが表示される。

### Phase 2: Conditions / Actions

- `FlowCondition` と `FlowAction` を追加。
- Transition成立時にaction列を実行する。
- `LoadScene` はAction経由にする。
- `SetFlag` / `ClearFlag` をRuntime flagsへ接続する。

受け入れ条件:

- `ui.button.clicked == StartGame` でScene遷移できる。
- `battle.ended == Victory` でResult Sceneへ遷移できる。
- `SetFlag` した値を別Transition条件で使える。

### Phase 3: BattleFlow構造改善

- `BattleFlowRuntime` またはpersistent方針を実装する。
- `BattleFlowComponent` をauthoring configとして整理する。
- BattleFlow phase transition時にeventを発行する。
- `StartBattleFlow` / `ResetBattleFlow` actionを実装する。

受け入れ条件:

- Scene load後もBattleFlow runtimeが消えない。
- GameFlowからBattle開始を指示できる。
- Battle終了時にGameFlowが勝敗で分岐できる。

### Phase 4: GameFlow Editor拡張

- Event Catalog追加。
- Condition list editor追加。
- Action list editor追加。
- Runtime debug panel追加。
- Node type `State` / `Battle` 追加。

受け入れ条件:

- Graph上でScene Node以外のState Nodeを作れる。
- Transitionに複数Conditionを追加できる。
- Transitionに複数Actionを追加できる。
- Validateが新仕様に対応する。

### Phase 5: Rename後の掃除

- 旧 `GameLoop*` 参照を削除する。
- 旧 `Data/GameLoop` 参照を削除する。
- 旧 `.gameloop` 保存/読み込み経路を残さない。
- 新 `GameFlow*` 名でinclude / project file / editor menuを統一する。

受け入れ条件:

- 新 `.gameflow` が保存できる。
- buildが通る。
- 既存Scene flowが壊れない。

---

## 15. 代表シナリオ

### 15.1 TitleのStartボタン

```text
UIButton Event Name: StartGame

GameFlow Transition:
  From: Title
  To: Gameplay
  Conditions:
    ui.button.clicked == StartGame
  Actions:
    Fade 0.15
    LoadScene Data/Scene/test.scene
```

### 15.2 Battle勝利でResultへ

```text
BattleFlow:
  Combat -> Victory
  emit battle.ended stringValue=Victory

GameFlow Transition:
  From: Gameplay
  To: Result
  Conditions:
    battle.ended == Victory
  Actions:
    Fade 0.30
    LoadScene Data/Scene/Result.scene
```

### 15.3 Battle敗北でRetry UIへ

```text
GameFlow Transition:
  From: Gameplay
  To: DefeatMenu
  Conditions:
    battle.ended == Defeat
  Actions:
    SetFlag playerDefeated = true
    EmitEvent flow.custom value=ShowDefeatMenu
```

---

## 16. 注意点

- GameFlowを万能システムにしない。GameFlowは「接続」と「進行」を担当し、各専門システムの中身は持たない。
- BattleFlowの中でScene loadしない。
- UIButton InspectorからTransition先を直接編集しない。
- Event nameは文字列なので、Editor側で候補表示とvalidationを必ず行う。
- 旧形式互換を前提にしない。
- Scene loadはframe境界で実行し、Action実行中にRegistryを破壊しない。
- BattleFlow eventはphase変更時に1回だけ出す。Victory/Defeat中に毎frame出さない。

---

## 17. 最初に実装すべき順番

1. UI表示名だけ `GameFlow Editor` に変更。
2. `FlowEvent` / `FlowEventQueue` を追加。
3. `UIButtonClickEventQueue` から `ui.button.clicked` を生成。
4. `BattleFlowSystem` から `battle.ended` を生成。
5. `GameFlowSystem` がFlowEventを条件評価に使う。
6. `LoadScene` をAction化。
7. GameFlow EditorにCondition list / Action listを追加。
8. BattleFlow runtime永続性を修正。

この順番なら、UIButton改善、GameFlow化、BattleFlow連携が一本の線でつながる。
