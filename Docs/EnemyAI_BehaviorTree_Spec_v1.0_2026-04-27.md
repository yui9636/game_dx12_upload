# Enemy AI / Behavior Tree Editor 仕様書 v1.0

作成日: 2026-04-27
改訂日: 2026-04-27 (レビュー指摘 #2 / #4 / #5 / #6 反映)
関連: `GameLoop_SceneTransition_Spec_v1.1_2026-04-26.md`、`PlayerEditor_StateMachine_Ownership_Spec_2026-04-25.md`、`PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md`

## 改訂履歴

### 2026-04-27 第 1 次レビュー反映

| ID | 指摘 | 反映箇所 |
|---|---|---|
| #6 | `AttackHeavy` が Player 仕様 (Attack 1 系統統合) と矛盾 | §4.2 / §5.3 / §13.2 / §18-19 / §25 から `AttackHeavy` 削除。`AttackBasic` を `Attack` に改名 |
| #5 | `MoveToTarget` が `LocomotionStateComponent.moveInput` (camera-relative) に world 方向を書くと向きズレ | §3.7 で `LocomotionStateComponent` に `useCameraRelativeInput` flag 追加を明文化。§5.3 で AI 用書込契約を確定 |
| #4 | `Attack` action が距離内で毎 frame 発火し続ける (連打バグ) | §5.3 / §5.6 で `Attack` を rising-edge セマンティクスに変更。`ActionStateComponent.state` 遷移完了まで Running、復帰で Success |
| #2 | `BehaviorTreeSystem` Query が `BlackboardComponent` を含むのに、禁止事項では「signature に含めない」と矛盾 | §21 の禁止文を削除し、§22 の分離規則として「Blackboard を読み書きするのは BT / Perception の 2 system のみ」に書き直し |

## 0. 目的

1 対 1 のアクションゲームを成立させるため、敵 (Enemy) の AI を **Behavior Tree** で authoring できるようにする。エディタ側は既存 `PlayerEditor` を拡張して **Editor Mode** を切り替える形で Enemy / NPC を扱う。Runtime 側は新しい AI 層 (BehaviorTreeSystem) を、既存の StateMachine / Locomotion / Timeline / Action / Health 各 system の **上位** にレイヤーとして追加する。

仕様書は **「候補」「想定」を排除し、すべて確定文** とする。実装担当者が判断を要する箇所はこの文書で予め決定する。

## 1. 範囲

### 1.1 やること (in scope)

- `BehaviorTreeAsset` (data) の追加
- `BehaviorTreeRuntimeComponent` / `BlackboardComponent` / `PerceptionComponent` / `AggroComponent` / `EnemyTagComponent` の追加
- `BehaviorTreeSystem` / `PerceptionSystem` の追加
- `EnemyConfigAsset` の追加 (BT + StateMachine + 数値設定の bundle)
- `PlayerEditor` の **Actor Editor Mode** 拡張 (`Player` / `Enemy` / `NPC`)
- 新規 dock panel: `BehaviorTreeEditorPanel` / `BlackboardPanel` / `PerceptionPanel`
- 標準 Enemy プリセット (Aggressive Knight)
- GameLoop の Battle scene で 1v1 が動く統合テスト

### 1.2 やらないこと (out of scope, v1.0 では延期)

- 複数 Enemy の協調 AI / team coordination
- HTN, GOAP, Utility AI
- Pathfinding (NavMesh) の本格統合 (`MoveTo` は直線移動のみ)
- 動的 BT 生成 / runtime 編集
- BT スクリプト言語 (graph editor のみ)
- Blackboard の cross-entity / global scope
- AI 視点での Replay 機能

### 1.3 既存の何を変更しないか

- `GameLoopSystem` / `SceneTransitionSystem` (動作不変)
- `PlayerInputSystem` (Player tagged entity に対する動作不変)
- `StateMachineSystem` (条件評価の挙動不変)
- `LocomotionSystem` / `ActionSystem` / `DodgeSystem` / `HealthSystem` (動作不変)
- `TimelineRuntimeSystem` (動作不変)
- `EngineKernel::Update` 内 GameLoop pipeline の位置

## 2. アーキテクチャ層

| 層 | 責務 | Asset | Component | System | 書く先 |
|---|---|---|---|---|---|
| **Decision** | 「何をすべきか」を決める | `BehaviorTreeAsset` | `BehaviorTreeAssetComponent`<br>`BehaviorTreeRuntimeComponent`<br>`BlackboardComponent` | `BehaviorTreeSystem` | `LocomotionStateComponent.moveInput`<br>`StateMachineParamsComponent` |
| **Perception** | 「何を見ているか」 | (なし) | `PerceptionComponent`<br>`AggroComponent` | `PerceptionSystem` | `BlackboardComponent`<br>`AggroComponent.target` |
| **Animation Selection** | アニメ state を選ぶ | `StateMachineAsset` | `StateMachineParamsComponent` | `StateMachineSystem` | `currentStateId` |
| **Locomotion** | 移動の実行 | (component 内) | `LocomotionStateComponent` | `LocomotionSystem` | `TransformComponent` |
| **Timeline** | hitbox / VFX / SE | `TimelineAsset` | (既存) | `TimelineRuntimeSystem` | hitbox events |
| **Combat** | ダメージ計算 | (なし) | `HealthComponent` | `HealthSystem`<br>`HitboxTrackingSystem` | `HealthComponent.health` |

データの流れ:

```
[Perception] -> Aggro/Blackboard -> [BehaviorTree] -> Params -> [StateMachine] -> stateId
                                                  -> moveInput -> [Locomotion] -> Transform
                                  Timeline runs in parallel with StateMachine
```

## 3. 新規 component (確定)

### 3.1 `EnemyTagComponent`

```cpp
// Source/Gameplay/EnemyTagComponent.h
struct EnemyTagComponent
{
    uint16_t enemyKindId = 0; // optional taxonomy id (knight=1, archer=2, ...). v1 では未使用でも可
};
```

- Player と同じく入力配線用ではなく **AI ルーティング識別** に使う。
- `ActorTypeComponent::type == Enemy` と並走する (ActorType は GameLoop 用、EnemyTag は AI 用)。

### 3.2 `BehaviorTreeAssetComponent`

```cpp
// Source/AI/BehaviorTreeAssetComponent.h
struct BehaviorTreeAssetComponent
{
    std::string assetPath;            // "Data/AI/BehaviorTrees/Knight.bt"
};
```

### 3.3 `BehaviorTreeRuntimeComponent`

```cpp
// Source/AI/BehaviorTreeRuntimeComponent.h
struct BehaviorTreeRuntimeComponent
{
    static constexpr int MAX_ACTIVE_STACK = 16;

    // 現在 Running を返した leaf までのパス (root から leaf まで)。
    uint32_t activeNodeStack[MAX_ACTIVE_STACK] = {};
    uint8_t  activeNodeStackDepth              = 0;

    // 各 node 用の汎用 timer / counter (id -> value)。
    // Wait / Cooldown / Repeat 用。
    // v1 では固定サイズ map で実装 (id 32 個まで)。
    static constexpr int MAX_NODE_STATE = 32;
    uint32_t nodeStateIds[MAX_NODE_STATE]    = {};
    float    nodeStateValues[MAX_NODE_STATE] = {};
    uint8_t  nodeStateCount                  = 0;

    // 直近 tick の各 node 結果 (debug overlay 用)。
    // id -> last status (None=0, Running=1, Success=2, Failure=3)
    static constexpr int MAX_DEBUG_TRACE = 64;
    uint32_t debugTraceIds[MAX_DEBUG_TRACE]    = {};
    uint8_t  debugTraceStatus[MAX_DEBUG_TRACE] = {};
    uint8_t  debugTraceCount                   = 0;

    // Asset の cache (path -> 読み込み済みの構造)。
    // Phase 1 では Update で path から asset を引いて使う。
};
```

### 3.4 `BlackboardComponent`

```cpp
// Source/AI/BlackboardComponent.h
enum class BlackboardValueType : uint8_t
{
    None     = 0,
    Bool     = 1,
    Int      = 2,
    Float    = 3,
    Vector3  = 4,
    Entity   = 5,
    String   = 6,
};

struct BlackboardValue
{
    BlackboardValueType type = BlackboardValueType::None;
    int                 i    = 0;
    float               f    = 0.0f;
    DirectX::XMFLOAT3   v3   { 0.0f, 0.0f, 0.0f };
    EntityID            entity = Entity::NULL_ID;
    std::string         s;
};

struct BlackboardComponent
{
    std::unordered_map<std::string, BlackboardValue> entries;
};
```

予約キー (system が読み書きする):

| key | type | 書く層 | 読む層 |
|---|---|---|---|
| `Self`         | Entity  | runtime init | BT (action params) |
| `Target`       | Entity  | PerceptionSystem | BT (action params) |
| `TargetPos`    | Vector3 | PerceptionSystem | BT |
| `TargetDist`   | Float   | PerceptionSystem | BT |
| `LastSeenTime` | Float   | PerceptionSystem | BT |
| `Health01`     | Float   | BT (helper) | BT |

### 3.5 `PerceptionComponent`

```cpp
// Source/AI/PerceptionComponent.h
struct PerceptionComponent
{
    // 視覚。
    bool  sightEnabled = true;
    float sightRadius  = 10.0f;     // metres
    float sightFOV     = 1.5708f;   // 90 deg
    float sightHeight  = 1.6f;      // ray origin offset (m)
    bool  requireLineOfSight = true;

    // 聴覚 (v1 は radius のみ、event は将来)。
    bool  hearingEnabled = false;
    float hearingRadius  = 6.0f;

    // 検出対象 (faction id)。0 = 全敵対 actor。
    uint16_t targetFactionMask = 0;
};
```

### 3.6 `AggroComponent`

```cpp
// Source/AI/AggroComponent.h
struct AggroComponent
{
    EntityID currentTarget   = Entity::NULL_ID;
    float    threat          = 0.0f;     // 0..1
    float    timeSinceSighted = 0.0f;     // sec
    float    loseTargetAfter = 5.0f;     // sec; 視界外でこの時間で target を失う
};
```

### 3.7 `LocomotionStateComponent` の拡張 (既存 component 改修)

AI は world 空間で「どの方向に進みたいか」を扱うので、camera-relative 変換を経由しない経路が必要。既存 `LocomotionStateComponent` (`Source/Gameplay/LocomotionStateComponent.h`) に **1 つの bool field を追加** する:

```cpp
struct LocomotionStateComponent
{
    DirectX::XMFLOAT2 moveInput = { 0, 0 };
    float             inputStrength = 0.0f;
    DirectX::XMFLOAT2 worldMoveDir  = { 0, 0 };
    // ... (既存フィールド省略)

    // ★ NEW: moveInput の解釈モード。
    //  true  = camera-relative stick 入力 (Player 用、既存挙動)。
    //  false = world-space x/z 方向 (AI 用)。LocomotionSystem は camera 変換をスキップする。
    bool useCameraRelativeInput = true;
};
```

`LocomotionSystem::Update` の修正契約 (該当 system 改修も必要):

```text
if (loco.useCameraRelativeInput) {
    worldX = cameraBasis.right.x * inputX + cameraBasis.forward.x * inputY;
    worldZ = cameraBasis.right.y * inputX + cameraBasis.forward.y * inputY;
} else {
    worldX = inputX;
    worldZ = inputY;
}
```

それ以外の挙動 (gait / 加速 / 旋回) は変えない。

`PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents` は `useCameraRelativeInput = true` を保証する。
`EnemyRuntimeSetup::EnsureAllEnemyRuntimeComponents` は `useCameraRelativeInput = false` を保証する。

## 4. BehaviorTreeAsset (確定)

### 4.1 ノードカテゴリ

```cpp
enum class BTNodeCategory : uint8_t
{
    Root       = 0,
    Composite  = 1,
    Decorator  = 2,
    Action     = 3,
    Condition  = 4,
};
```

### 4.2 ノードタイプ列挙 (v1.0 で扱うもの)

```cpp
enum class BTNodeType : uint16_t
{
    Root              = 0,

    // Composite
    Sequence          = 100,
    Selector          = 101,
    Parallel          = 102,

    // Decorator
    Inverter          = 200,
    Repeat            = 201,
    Cooldown          = 202,
    ConditionGuard    = 203,    // 子の前段 condition

    // Condition
    HasTarget         = 300,
    TargetInRange     = 301,
    TargetVisible     = 302,
    HealthBelow       = 303,
    StaminaAbove      = 304,
    BlackboardEqual   = 305,

    // Action (locomotion / movement)
    Wait              = 400,
    FaceTarget        = 401,
    MoveToTarget      = 402,
    StrafeAroundTarget= 403,
    Retreat           = 404,

    // Action (combat)
    // 注: Player 仕様 (PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md §3) に揃え、
    //     攻撃は Light/Heavy に分けず Attack 1 系統で統一する。
    Attack            = 500,
    DodgeAction       = 502,

    // Action (state-machine I/F)
    SetSMParam        = 600,    // generic StateMachine param 書込
    PlayState         = 601,    // 直接 currentStateId を書く (危険なので限定使用)

    // Action (blackboard)
    SetBlackboard     = 700,
};
```

### 4.3 ノードデータ

```cpp
struct BTNode
{
    uint32_t          id            = 0;
    BTNodeType        type          = BTNodeType::Root;
    std::string       name;          // editor 用ラベル
    std::vector<uint32_t> childrenIds;

    // 数値 / 文字列パラメータ (type ごとに使い分け)
    float    fParam0 = 0.0f;          // ex: TargetInRange.range, Wait.seconds, Cooldown.t
    float    fParam1 = 0.0f;          // ex: Retreat.minDistance
    int      iParam0 = 0;             // ex: Repeat.count, Parallel.successThreshold
    std::string sParam0;              // ex: SetSMParam.paramName, BlackboardEqual.key
    std::string sParam1;              // ex: SetBlackboard.key
    float       fParam2 = 0.0f;        // ex: SetBlackboard.value (when float)
    BlackboardValueType bbType = BlackboardValueType::None;

    // Editor layout
    DirectX::XMFLOAT2 graphPos { 0.0f, 0.0f };
};
```

### 4.4 アセット構造

```cpp
struct BehaviorTreeAsset
{
    int                  version  = 1;
    uint32_t             rootId   = 0;
    std::vector<BTNode>  nodes;

    const BTNode* FindNode(uint32_t id) const;
    BTNode*       FindNode(uint32_t id);
    uint32_t      AllocateNodeId() const;

    static BehaviorTreeAsset CreateAggressiveTemplate();
    static BehaviorTreeAsset CreateDefensiveTemplate();
    static BehaviorTreeAsset CreatePatrolTemplate();

    bool LoadFromFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path) const;
};
```

### 4.5 Tree 構造の制約 (validate でチェック)

| ルール | severity |
|---|---|
| Root が 1 個 | Error |
| Root の `childrenIds.size() == 1` | Error |
| Composite の `childrenIds.size() >= 1` | Error |
| Decorator の `childrenIds.size() == 1` | Error |
| Action / Condition の `childrenIds.size() == 0` | Error |
| 全 node id がユニーク | Error |
| 子の id が `nodes[]` に存在 | Error |
| グラフが DAG (Tree) かつ Root から全 node に到達可能 | Error |
| Cooldown.t > 0 | Error |
| Repeat.count > 0 | Error |
| Wait.seconds > 0 | Error |
| Parallel.successThreshold ∈ [1, 子数] | Error |
| TargetInRange.range > 0 | Error |
| HealthBelow.threshold ∈ (0, 1] | Warning |

## 5. Tick 実行セマンティクス (確定)

### 5.1 戻り値

```cpp
enum class BTStatus : uint8_t
{
    None    = 0,
    Running = 1,
    Success = 2,
    Failure = 3,
};
```

### 5.2 Tick 関数シグネチャ

```cpp
struct BTContext
{
    Registry&                     gameRegistry;
    Registry&                     gameLoopRegistry;   // global / persistent
    EntityID                      selfEntity;
    BehaviorTreeRuntimeComponent& runtime;
    BlackboardComponent&          blackboard;
    AggroComponent*               aggro;              // optional
    PerceptionComponent*          perception;         // optional
    float                         dt;
};

BTStatus Tick(const BehaviorTreeAsset& asset, const BTNode& node, BTContext& ctx);
```

### 5.3 ノード別セマンティクス

| Node | 動作 |
|---|---|
| `Root` | 子 1 つを Tick して結果をそのまま返す |
| `Sequence` | 子を順に Tick。最初に `Failure` で `Failure`、`Running` で `Running` (次フレーム同 index から再開)、全員 `Success` なら `Success` |
| `Selector` | 子を順に Tick。最初に `Success` で `Success`、`Running` で `Running` (次フレーム同 index から再開)、全員 `Failure` なら `Failure` |
| `Parallel` | 全子を Tick。`Success` 数 ≥ `iParam0` なら `Success`、`Failure` 数 ≥ (子数 - `iParam0` + 1) なら `Failure`、それ以外 `Running` |
| `Inverter` | 子の `Success` ⇄ `Failure` を反転、`Running` はそのまま |
| `Repeat` | 子を Tick。`Success` で count++、count == `iParam0` なら `Success`、count < `iParam0` なら `Running`、子が `Failure` なら `Failure` |
| `Cooldown` | 最後の `Success` から `fParam0` 秒未満なら `Failure`、それ以外は子を Tick (子の結果を返す。`Success` 時は時刻を記録) |
| `ConditionGuard` | `sParam0` の condition node id を tick。`Success` なら子を Tick して結果を返す。`Failure` なら子を tick せず `Failure` |
| `HasTarget` | `aggro && aggro->currentTarget != NULL_ID` なら `Success` |
| `TargetInRange` | `Target` blackboard の TransformComponent を取り、自分との距離 ≤ `fParam0` なら `Success` |
| `TargetVisible` | `PerceptionSystem` が直近フレームで visible 判定したなら `Success` (`LastSeenTime` が 0 と等しい) |
| `HealthBelow` | `HealthComponent.health / maxHealth < fParam0` なら `Success` |
| `StaminaAbove` | `StaminaComponent.value > fParam0` なら `Success` (StaminaComponent が無いなら `Failure`) |
| `BlackboardEqual` | `blackboard[sParam0]` が type に応じて等価なら `Success` |
| `Wait` | nodeState[id].timer += dt。timer < `fParam0` なら `Running`、≥ なら `Success` (timer リセット) |
| `FaceTarget` | `Target` の方向に `LocomotionStateComponent.targetAngleY` を設定。1 frame で `Success` |
| `MoveToTarget` | §5.5 の world-space 入力契約に従い `LocomotionStateComponent.moveInput` を **world x/z** で設定し続ける。`useCameraRelativeInput == false` を前提とする。距離 ≤ `fParam0` (stopRange) なら `Success`、それまで `Running`。Target lost で `Failure` |
| `StrafeAroundTarget` | `Target` を中心に円運動。`moveInput` は world tangent。`fParam0` 秒継続後 `Success` |
| `Retreat` | Target から逆方向 world-space に移動。距離 ≥ `fParam0` (minDist) で `Success` |
| `Attack` | §5.6 の rising-edge セマンティクスに従う。`Locomotion → Action → Locomotion` のサイクル全体で 1 ノード呼出をカバーし、サイクル完了で `Success`、`Damage / Dead` で `Failure` |
| `DodgeAction` | `Attack` と同じ rising-edge セマンティクスを `Dodge` parameter で実施。`Locomotion → Dodge → Locomotion` のサイクル完了で `Success` |
| `SetSMParam` | `StateMachineParamsComponent.SetParam(sParam0, fParam0)`。1 frame で `Success` |
| `PlayState` | (危険) state name で StateMachine の currentStateId を書く。1 frame で `Success` |
| `SetBlackboard` | `blackboard[sParam1]` に書く。`Success` |

### 5.4 Active leaf 管理 (Running の継続)

`BehaviorTreeRuntimeComponent.activeNodeStack` は **直近 frame で `Running` を返した leaf までのパス** (root から leaf まで)。

次フレームは:

1. stack が空 → root から普通に tick
2. stack が空でない → stack[0] (root) から下って、stack[depth-1] が leaf。途中の Sequence/Selector は **stack 上の child index から再開** する。
3. tick 中に高優先度の `Selector` 子で別 path が `Success` を返した場合、**現在の stack を abort** し、新 path で `Running` を立てる。

`Running` を返した node が次フレームで再 tick される時、Sequence/Selector は **保存された child index から開始** すること。`activeNodeStackDepth = 0` で常に root から開始。

### 5.5 移動系 action の入力契約 (world-space で書く)

`MoveToTarget` / `StrafeAroundTarget` / `Retreat` / その他 BT が直接駆動する移動系 action は **world space x/z 方向** を `LocomotionStateComponent.moveInput` に書く。

前提:

- 当該 entity の `LocomotionStateComponent.useCameraRelativeInput == false` であること (`EnemyRuntimeSetup` が保証)。
- `inputStrength` は `[0, 1]` で同時に書く (停止時 0、最大歩行 0.5、走行 1.0 など action 側で決定)。
- `targetAngleY` は world yaw (ラジアン、+X 方向 = 0、左回りで増)。

書き方の確定:

```text
dir = normalize(targetWorldPos.xz - selfWorldPos.xz)
loco.moveInput.x  = dir.x
loco.moveInput.y  = dir.y      // 注: y フィールドは z 軸方向に対応 (LocomotionStateComponent の既存規約)
loco.inputStrength = desiredStrength
loco.targetAngleY = atan2(-dir.y, dir.x)   // LocomotionSystem の yaw 規約に合わせる
```

LocomotionSystem は `useCameraRelativeInput == false` の場合、camera basis をかけずに `moveInput` を直接 world 方向として扱う (§3.7 の改修条件)。

BT action は **camera 方向に依存しない**。これにより main camera が無い scene / cinematic シーン中でも AI が正しく動く。

### 5.6 Attack / DodgeAction の rising-edge セマンティクス

問題: `Attack` を毎 tick で `SetParam("Attack", 1.0f)` し続けると、Locomotion → Action 遷移直後に Action → Locomotion へ復帰したフレームで再び 1 が立ち、無限連打になる。

解決: `Attack` (および `DodgeAction`) は **`ActionStateComponent.state` の遷移サイクル全体で 1 ノード呼出をカバー** する stateful action とする。

状態遷移は次の表で確定:

| 入った時の `state` | 内部 phase の遷移 | この tick で書く param | 戻り値 |
|---|---|---|---|
| `Locomotion` (phase 0) | phase 0 → 1 | `SetParam("Attack", 1.0f)` | `Running` |
| `Locomotion` (phase 1, 直前 tick で要求送信済み、SM が未遷移) | phase 1 維持 | `SetParam("Attack", 1.0f)` | `Running` |
| `Action`     (phase 1 / 2) | phase → 2 | (書かない) | `Running` |
| `Locomotion` (phase 2, Action 完了後の復帰) | phase 2 → 0 (リセット) | (書かない) | `Success` |
| `Damage` / `Dead` (任意 phase) | phase → 0 (リセット) | (書かない) | `Failure` |

実装規約:

- 内部 phase は `BehaviorTreeRuntimeComponent.nodeStateValues[]` に node id をキーに 1 値で保持する (`0=Idle, 1=Requested, 2=InProgress`)。
- `BehaviorTreeSystem::Update` の冒頭で `Attack / Dodge` parameter を 0 にクリアする処理は **削除しない**。`Attack` action が phase 0/1 でのみ 1 を書き、phase 2 では書かないため、SM は rising edge を 1 度だけ観測する。
- `DodgeAction` も同じ規約 (`ActionStateComponent.state == Dodge` を待つ)。

これにより:

- `TargetInRange → Attack` の Sequence は、`Attack` が `Running` を返す間 Sequence も `Running`。
- 攻撃完了で Sequence 全体が `Success`、その frame で Selector の優先度評価が再度回り、再度 `TargetInRange` が成立すれば次の Attack に入る (= 連打ではなく **1 攻撃 = 1 サイクル** の制御)。
- 攻撃中に Player が範囲外に出ても、`Attack` は `state == Action` の間 `Running` を返し続けるので、攻撃モーション途中でキャンセルされない (cancel 機能は別 action として将来追加)。

### 5.7 Tick 周波数

- 1 frame に 1 回 tick (gameplay frame と同じ)。
- `EngineMode::Pause` 時は tick しない (`activeNodeStack` は保持)。
- `EngineMode::Editor` 時は tick しない (`Reset` する)。

## 6. Frame phase 順序 (確定)

`GameLayer::Update` 内の system 順序を次のとおりに **追加** する。新規 system は ★ 印。

```text
InputContextSystem
InputResolveSystem
InputTextSystem
InputFeedbackSystem

PerceptionSystem            ★ NEW (Aggro + Blackboard 更新)
PlayerInputSystem           (Player tagged のみ。既存)
BehaviorTreeSystem          ★ NEW (Enemy tagged のみ)

PlaybackSystem
StateMachineSystem
LocomotionSystem
StaminaSystem
HealthSystem
DodgeSystem
CharacterPhysicsSystem
TimelineSystem
ActionSystem
... (以下既存順)
```

理由:

- PerceptionSystem は `BehaviorTreeSystem` より前 (BT が同 frame で見えた敵を判断する)。
- BehaviorTreeSystem は `StateMachineSystem` より前 (params を書く)。
- BehaviorTreeSystem は `LocomotionSystem` より前 (moveInput を書く)。
- BehaviorTreeSystem は `PlayerInputSystem` の **後ろ** (順序は実害なし、ただし debug 時に "プレイヤー入力 → AI 反応" と読める順序にする)。

## 7. PerceptionSystem (確定)

```cpp
class PerceptionSystem
{
public:
    static void Update(Registry& registry, float dt);
};
```

### 7.1 処理手順

1. `EnemyTagComponent + PerceptionComponent + AggroComponent + TransformComponent + BlackboardComponent` を持つ各 entity について:
2. 候補 target を `ActorTypeComponent::type == Player` の actor からスキャン (factionMask が 0 でなければそれにマッチする faction の actor も)。
3. 各候補 target に対し:
   - 距離 ≤ `sightRadius`
   - 自分の forward vector との角度 ≤ `sightFOV / 2`
   - `requireLineOfSight` なら raycast (collision system に問い合わせ。Phase 1 は省略可、distance のみ)
   - 全部満たすなら `aggro.currentTarget = target`、`aggro.timeSinceSighted = 0`、blackboard に `Target / TargetPos / TargetDist / LastSeenTime` を書く。
4. 何も見えない場合: `aggro.timeSinceSighted += dt`。`timeSinceSighted > loseTargetAfter` なら `aggro.currentTarget = NULL_ID`。
5. blackboard 更新は **ECS での dirty flag を立てない** (毎 frame 書き換える前提)。

### 7.2 PerceptionSystem の禁止事項

- Player 側 component の **書き換え** をしない (read only)。
- BT runtime に直接書き込まない (blackboard 経由のみ)。
- 効果音 / VFX を起こさない。

## 8. BehaviorTreeSystem (確定)

```cpp
class BehaviorTreeSystem
{
public:
    static void Update(Registry& registry, float dt);
    static void InvalidateAssetCache(const char* path = nullptr);
};
```

### 8.1 処理手順

```cpp
void BehaviorTreeSystem::Update(Registry& registry, float dt)
{
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) return;

    Query<EnemyTagComponent,
          BehaviorTreeAssetComponent,
          BehaviorTreeRuntimeComponent,
          BlackboardComponent,
          StateMachineParamsComponent> q(registry);

    q.ForEach([&](EntityID self,
                  EnemyTagComponent&,
                  BehaviorTreeAssetComponent& assetComp,
                  BehaviorTreeRuntimeComponent& runtime,
                  BlackboardComponent& blackboard,
                  StateMachineParamsComponent& smParams)
    {
        const BehaviorTreeAsset* asset = LoadOrCache(assetComp.assetPath);
        if (!asset) return;

        BTContext ctx{ registry, EngineKernel::Instance().GetGameLoopRegistry(),
                       self, runtime, blackboard,
                       registry.GetComponent<AggroComponent>(self),
                       registry.GetComponent<PerceptionComponent>(self),
                       dt };

        const BTNode* root = asset->FindNode(asset->rootId);
        if (!root) return;

        // 直近 frame の Action 系 "1-frame trigger param" を 0 に戻す。
        // 5.6 の rising-edge セマンティクスにより、Attack action 自身が
        // phase 0/1 で 1 を書き直す。phase 2 (in progress) では書かないので
        // SM は最初の 1 frame のみ rising edge を観測する。
        smParams.SetParam("Attack", 0.0f);
        smParams.SetParam("Dodge", 0.0f);

        // tick.
        runtime.debugTraceCount = 0;
        Tick(*asset, *root, ctx);
    });
}
```

### 8.2 BehaviorTreeSystem の禁止事項

- 自分以外の entity の **gameplay component** を書き換えない (Aggro / Blackboard は OK)。
- `currentStateId` を直接書かない (`PlayState` action 以外)。
- `PrefabSystem` / `Registry::CreateEntity` を呼ばない。
- `EditorLayer` を直接操作しない。
- `EngineKernel::Mode` を変えない。

## 9. EnemyConfigAsset (バンドル)

```cpp
struct EnemyConfigAsset
{
    int          version = 1;
    std::string  name;

    // Asset 参照。
    std::string  behaviorTreePath;
    std::string  stateMachinePath;
    std::string  timelinePath;
    std::string  modelPath;
    std::string  animatorPath;

    // Stats.
    float        maxHealth     = 100.0f;
    float        walkSpeed     = 2.0f;
    float        runSpeed      = 4.5f;
    float        turnSpeed     = 540.0f;   // deg/sec

    // Perception default.
    float        sightRadius   = 10.0f;
    float        sightFOV      = 1.5708f;
    float        hearingRadius = 0.0f;

    // Combat.
    float        baseAttack    = 10.0f;

    bool LoadFromFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path) const;
};
```

EnemyConfigAsset は **読み取り専用の bundle**。実 entity 構築は `EnemyRuntimeSetup` (後述) が行う。

## 10. EnemyRuntimeSetup (確定)

```cpp
namespace EnemyRuntimeSetup
{
    // Editor / Play 開始時に Enemy entity を runtime 動作可能な状態にする。
    // Player の PlayerRuntimeSetup と対称な役割。
    void EnsureAllEnemyRuntimeComponents(Registry& registry, bool resetState);

    // EnemyConfig をもとに 1 entity を構築 (scene serialize でも、in-editor spawn でも使う)。
    EntityID SpawnFromConfig(Registry& registry, const EnemyConfigAsset& config,
                             const DirectX::XMFLOAT3& position);
}
```

`EnsureAllEnemyRuntimeComponents` が `EnemyTagComponent` を持つ entity に必須 component を付け足す:

- `ActorTypeComponent { Enemy }`
- `LocomotionStateComponent`
- `ActionStateComponent`
- `StateMachineParamsComponent`
- `HealthComponent`
- `BehaviorTreeAssetComponent` (空 path なら warning)
- `BehaviorTreeRuntimeComponent`
- `BlackboardComponent`
- `PerceptionComponent`
- `AggroComponent`

## 11. Serialization (確定 — JSON)

### 11.1 `.bt` 形式

```json
{
  "version": 1,
  "rootId": 1,
  "nodes": [
    { "id": 1, "type": "Root", "name": "Root", "children": [2] },
    { "id": 2, "type": "Selector", "name": "TopSelect", "children": [3, 6] },
    { "id": 3, "type": "Sequence", "name": "AttackIfClose", "children": [4, 5] },
    { "id": 4, "type": "TargetInRange", "name": "InRange?", "fParam0": 2.0 },
    { "id": 5, "type": "Attack",      "name": "Attack" },
    { "id": 6, "type": "MoveToTarget", "name": "Chase", "fParam0": 1.5 }
  ]
}
```

- `children` は配列順そのまま。Sequence/Selector の優先度はこの並び順。
- `graphPos` は省略可 (editor 表示専用)。

### 11.2 `.enemy` 形式

```json
{
  "version": 1,
  "name": "AggressiveKnight",
  "behaviorTreePath": "Data/AI/BehaviorTrees/Knight.bt",
  "stateMachinePath": "Data/Scenes/SM/Knight.sm",
  "timelinePath":     "Data/Scenes/TL/Knight.tl",
  "modelPath":        "Data/Models/Knight.gltf",
  "animatorPath":     "Data/Animator/Knight.anim",
  "maxHealth": 100.0,
  "walkSpeed": 2.0,
  "runSpeed":  4.5,
  "turnSpeed": 540.0,
  "sightRadius": 10.0,
  "sightFOV":    1.5708,
  "hearingRadius": 0.0,
  "baseAttack": 10.0
}
```

### 11.3 ファイル配置

| 種別 | ディレクトリ | 拡張子 |
|---|---|---|
| Behavior Tree | `Data/AI/BehaviorTrees/` | `.bt` |
| Enemy Config  | `Data/AI/Enemies/`       | `.enemy` |
| Blackboard schema (Phase 2) | `Data/AI/Blackboards/` | `.bb` |

## 12. PlayerEditor 拡張 (確定)

### 12.1 `ActorEditorMode`

```cpp
enum class ActorEditorMode : uint8_t
{
    Player = 0,
    Enemy  = 1,
    NPC    = 2,
};
```

`PlayerEditorPanel` に次のメンバを追加:

```cpp
ActorEditorMode m_actorEditorMode = ActorEditorMode::Player;

BehaviorTreeAsset    m_behaviorTreeAsset;
EnemyConfigAsset     m_enemyConfigAsset;
bool                 m_behaviorTreeDirty = false;
bool                 m_enemyConfigDirty  = false;
```

### 12.2 toolbar に mode dropdown を追加

```text
[Mode: Player ▼] [Skeleton] [StateMachine] [BehaviorTree] [Save] [Save All]
       Player
       Enemy
       NPC
```

### 12.3 layout 切替表

| Panel | Player mode | Enemy mode | NPC mode |
|---|---|---|---|
| Skeleton           | ✓ | ✓ | ✓ |
| StateMachine       | ✓ | ✓ | ✓ |
| Timeline           | ✓ | ✓ | ✓ |
| Properties         | ✓ | ✓ | ✓ |
| Animator           | ✓ | ✓ | ✓ |
| Input Mapping      | ✓ | ✗ | ✗ |
| **BehaviorTree**   | ✗ | ✓ | ✓ |
| **Blackboard**     | ✗ | ✓ | ✓ |
| **Perception**     | ✗ | ✓ | ✓ |
| **EnemyStats**     | ✗ | ✓ | ✗ |

✓ は表示・編集可能、✗ は dock layout から外す。

### 12.4 mode 切替時の挙動

- 切替前に dirty があれば save 確認 dialog。
- 切替時に dock layout を再構築 (`m_needsLayoutRebuild = true`)。
- preview entity が `EnemyTagComponent` を持つよう更新する (Player→Enemy 切替時)。
- preview entity の `PlayerTagComponent` は逆切替時に削除する。
- `EnsureAllEnemyRuntimeComponents` を Enemy 切替時に呼ぶ。

## 13. BehaviorTreeEditorPanel (確定)

```cpp
// Source/AI/BehaviorTreeEditorPanel.h
class BehaviorTreeEditorPanel
{
public:
    void Draw(BehaviorTreeAsset& asset,
              BehaviorTreeRuntimeComponent* runtime,   // optional, debug 用
              bool* outDirty);

private:
    void DrawToolbar(BehaviorTreeAsset& asset);
    void DrawTreeView(BehaviorTreeAsset& asset);
    void DrawNodeInspector(BehaviorTreeAsset& asset);
    void DrawValidateResult();

    int      m_selectedNodeIndex = -1;
    char     m_loadPathBuf[256]  = {};
    char     m_saveAsPathBuf[256] = {};
    GameLoopValidateResult m_lastValidate; // 流用 (severity 共通)
    bool     m_validatedOnce = false;
};
```

### 13.1 Tree view

- ルートから indent でツリー表示。
- 各 node に icon + 名前 (例: `[SEQ]`, `[SEL]`, `[ATK]`, `[?]`)。
- runtime が non-null なら、直近 tick の `debugTraceStatus` から色を変える:
  - Running = 黄
  - Success = 緑
  - Failure = 赤
  - None    = グレー
- 右クリックメニュー: Add Child / Duplicate / Delete / Convert Type (互換時のみ)
- ドラッグで子順序を入れ替え可能。

### 13.2 Node Inspector

| Type | UI |
|---|---|
| `Root` | (フィールドなし) |
| `Sequence` / `Selector` | (フィールドなし) |
| `Parallel` | `successThreshold` (int) |
| `Inverter` | (フィールドなし) |
| `Repeat` | `count` (int, > 0) |
| `Cooldown` | `seconds` (float, > 0) |
| `ConditionGuard` | `conditionNodeId` (dropdown of Condition nodes) |
| `HasTarget` | (フィールドなし) |
| `TargetInRange` | `range` (float) |
| `TargetVisible` | (フィールドなし) |
| `HealthBelow` | `threshold` (float, 0..1) |
| `StaminaAbove` | `threshold` (float) |
| `BlackboardEqual` | `key` (str), `bbType` (dropdown), `value` (type 別) |
| `Wait` | `seconds` (float) |
| `FaceTarget` | (フィールドなし) |
| `MoveToTarget` | `stopRange` (float) |
| `StrafeAroundTarget` | `seconds` (float) |
| `Retreat` | `minDistance` (float) |
| `Attack` / `DodgeAction` | (フィールドなし) |
| `SetSMParam` | `paramName` (str), `value` (float) |
| `PlayState` | `stateName` (str) |
| `SetBlackboard` | `key` (str), `bbType` (dropdown), `value` (type 別) |

### 13.3 Toolbar

- `New Default Loop` 相当の `New Aggressive Template` / `New Defensive Template` / `New Patrol Template`
- `Validate` / `Save` / `Load` / `Save As`

## 14. BlackboardPanel / PerceptionPanel / EnemyStatsPanel

### 14.1 `BlackboardPanel`

- 現 entity の `BlackboardComponent.entries` を一覧表示。
- 編集可。Enemy mode の Editor で初期値を設定する用途。
- key 追加 / 削除 / type 変更ボタン。

### 14.2 `PerceptionPanel`

- `PerceptionComponent` の sliders。
- Viewport には sight cone を gizmo 描画 (Phase 1 では半円で代用可)。

### 14.3 `EnemyStatsPanel`

- `EnemyConfigAsset` の field を直接編集。
- `Save Enemy` で `.enemy` に書き出し。

## 15. Validate (確定)

`BehaviorTreeAsset` 用に `ValidateBehaviorTree(asset)` を実装する。エラー / 警告基準は §4.5。GameLoop の validate と同じ severity 体系を流用。

実行タイミング:

| トリガ | 実行内容 |
|---|---|
| `[Validate]` ボタン | 全ルール |
| `Save` 実行時 | 全ルール、Error 時は abort + dialog |
| `Play` 押下時 | 全 enemy entity の `BehaviorTreeAssetComponent.assetPath` を validate。Error なら警告 (進行は止めない) |

## 16. 受け入れ条件 (Phase 全完了時)

- [ ] PlayerEditor の toolbar で `Player / Enemy / NPC` を切り替えられる
- [ ] Enemy mode で BehaviorTree / Blackboard / Perception / EnemyStats panel が表示される
- [ ] BehaviorTreeEditorPanel から `New Aggressive Template` で 6 node のツリーを生成できる
- [ ] `.bt` を Save / Load 後に同一構造で復元される
- [ ] `.enemy` を Save / Load 後に同一値で復元される
- [ ] Battle scene に Enemy entity を 1 体配置 (= 1v1)
- [ ] Play 押下で Enemy が Player を `PerceptionSystem` で検知し、`AggroComponent.currentTarget` に Player が入る
- [ ] BehaviorTreeSystem が tick され、Aggressive Template の場合:
  - Player との距離 > 2m → `MoveToTarget` で Player に近づく (world-space 入力、§5.5)
  - 距離 ≤ 2m → `Attack` で `StateMachineParamsComponent.SetParam("Attack", 1.0f)` を rising-edge (§5.6) で送信
  - StateMachine が Attack state に遷移し、Timeline の hitbox が Player に当たって Player の health が減る
  - `Attack` 1 サイクル中は再要求しない (連打しない)。サイクル完了後、距離内なら次の Attack が始まる
- [ ] Editor で BT panel の debug overlay が「現在 tick されている node」を黄色で表示する
- [ ] `Pause` で BT tick が止まる、`activeNodeStack` は保持される
- [ ] `Stop` で BT runtime が reset される
- [ ] `GameLoop` の `AllActorsDead Enemy` condition が、Enemy 全滅で発火する
- [ ] `EnemyConfigAsset` の `walkSpeed` を変更すると、scene 内 Enemy の最大歩行速度が変わる

## 17. Phase 計画

### Phase 1: データ基盤

完成条件:

- `Source/AI/` ディレクトリ新設
- `BehaviorTreeAsset.{h,cpp}` (struct + JSON load/save + validate + 3 templates)
- `BehaviorTreeAssetComponent.h`
- `BehaviorTreeRuntimeComponent.h`
- `BlackboardComponent.h`
- `PerceptionComponent.h`
- `AggroComponent.h`
- `EnemyTagComponent.h` (`Source/Gameplay/`)
- `EnemyConfigAsset.{h,cpp}`
- Game.vcxproj 登録
- ComponentMeta auto-gen が新 component を取り込む確認

### Phase 2: Runtime tick

完成条件:

- `BehaviorTreeSystem.{h,cpp}` (Tick 実装、`Sequence`/`Selector`/`Parallel`/`Inverter`/`Repeat`/`Cooldown`/`ConditionGuard` + 全 Condition + 全 Action)
- **`LocomotionStateComponent` に `useCameraRelativeInput` を追加** (§3.7)、`LocomotionSystem` の解釈分岐を実装
- **`Attack` / `DodgeAction` action を rising-edge セマンティクスで実装** (§5.6、`BehaviorTreeRuntimeComponent.nodeStateValues[]` で phase 管理)
- **`MoveToTarget` / `StrafeAroundTarget` / `Retreat` は world-space 入力契約** (§5.5)
- `PerceptionSystem.{h,cpp}` (sight 距離 + FOV、raycast は Phase 5 で)
- `EnemyRuntimeSetup.{h,cpp}` (`EnsureAllEnemyRuntimeComponents` + `SpawnFromConfig`)。`useCameraRelativeInput = false` の保証を含む
- `PlayerRuntimeSetup` 側で `useCameraRelativeInput = true` を保証 (既存 entity 後方互換)
- `GameLayer::Update` への system 呼び出し追加 (§6 順序)
- 受け入れ条件 §16 のうち、editor 部分以外を満たす

### Phase 3: Editor — Mode 切替

完成条件:

- `PlayerEditorPanel` に `ActorEditorMode` 追加
- toolbar に mode dropdown
- mode 切替時の dock layout 再構築
- preview entity の component 切替 (Player Tag ↔ Enemy Tag)
- mode による panel 表示切替
- 受け入れ条件 §16 の panel 表示が正しい

### Phase 4: Editor — Behavior Tree Panel

完成条件:

- `BehaviorTreeEditorPanel.{h,cpp}` 実装
- tree view 表示 + 選択
- node inspector (全 type 対応)
- 子の drag-reorder
- New Template / Save / Load / Save As / Validate
- runtime debug overlay (status 色分け)

### Phase 5: Editor — Blackboard / Perception / EnemyStats Panel

完成条件:

- `BlackboardPanel.{h,cpp}` (key-value 編集)
- `PerceptionPanel.{h,cpp}` (slider + sight cone gizmo)
- `EnemyStatsPanel.{h,cpp}` (`.enemy` の値を編集 + save)

### Phase 6: 拡張 perception (任意)

- raycast によるオクルージョン判定
- hearing event (footstep / weapon swing)
- threat 計算 (距離 + 視線 + ダメージ履歴)

### Phase 7: 統合テスト用 1v1

- 標準 Battle scene 更新: Player + Enemy (Aggressive Knight)
- 標準 BT: AggressiveTemplate
- 標準 EnemyConfig: Knight
- GameLoop: Title → Battle → (Enemy 全滅) → Result
- 動作確認 § 16 全条件成立

## 18. 標準テンプレート (Aggressive)

```text
Root
└─ Selector
   ├─ Sequence "AttackIfClose"
   │  ├─ TargetInRange (range=2.0)
   │  └─ Attack                     ; rising-edge セマンティクス (§5.6)
   ├─ Sequence "ChaseIfVisible"
   │  ├─ HasTarget
   │  └─ MoveToTarget (stopRange=1.5)   ; world-space 入力 (§5.5)
   └─ Wait (seconds=1.0)
```

注: `Attack` は state machine の `Locomotion → Action → Locomotion` 1 サイクル全体で 1 回の Tick 結果を返すため、`AttackIfClose` Sequence は攻撃モーション中ずっと `Running` を返し続け、Selector は他枝に移らない。攻撃完了で `Success`、その frame で再度 Selector が評価され、`TargetInRange` が成立すれば次の Attack に入る (連打ではなく **1 攻撃 = 1 サイクル**)。

## 19. 標準テンプレート (Defensive)

```text
Root
└─ Selector
   ├─ Sequence "Retreat"
   │  ├─ HealthBelow (0.3)
   │  └─ Retreat (minDistance=4.0)
   ├─ Sequence "AttackInRange"
   │  ├─ TargetInRange (3.0)
   │  └─ Attack
   ├─ Sequence "Strafe"
   │  ├─ HasTarget
   │  └─ StrafeAroundTarget (seconds=2.0)
   └─ Wait (1.0)
```

## 20. 標準テンプレート (Patrol)

```text
Root
└─ Sequence
   ├─ Wait (1.0)
   └─ MoveToTarget (stopRange=0.5)         ; Target = waypoint (Phase 2 で waypoint 機構を追加)
```

(Patrol は v1.0 では未完成。`Target` を waypoint にする仕組みは Phase 6 以降)

## 21. 禁止事項 (統合)

- 既存 `PlayerInputSystem` の挙動を Enemy entity に対しても発火させること (`PlayerTagComponent` を持つ entity のみで動作することを保つ)
- `BehaviorTreeSystem` から `PrefabSystem` を呼ぶこと
- `BehaviorTreeSystem` から `Registry::DestroyEntity` を呼ぶこと (敵の死亡は `HealthSystem` 経由)
- `BehaviorTreeSystem` から `EditorLayer` / `EngineKernel::Mode` を操作すること
- `PerceptionSystem` で player 側 component を書き換えること
- `BehaviorTreeSystem` / `PerceptionSystem` **以外の system** が `BlackboardComponent` を読み書きすること (この 2 system のみを Blackboard の owner とする。後述 §22 参照)
- `BehaviorTreeAsset` を tick 中に mutate すること
- Enemy 用に新たな `Mode` を `EngineKernel` に作ること (Editor / Play / Pause で十分)
- Enemy の input mapping を `InputActionMapAsset` で扱うこと (BT が直接 SM params に書く)
- `GameLoop` の `Battle` scene を Enemy 専用 hard-coded class にすること (asset 駆動を維持)
- BT node の condition 評価を 1 frame に複数回行うこと (1 tick で各 node は最大 1 回)
- BT action から **他 entity の gameplay component** を書き換えること (自 entity と Blackboard / Aggro 経由のみ許可)
- AI 側 entity の `LocomotionStateComponent.useCameraRelativeInput` を `true` のまま放置すること (`EnemyRuntimeSetup` で必ず `false` にする)

注: §8.1 の `BehaviorTreeSystem` の Query が `BlackboardComponent` を signature に含めるのは **意図通り**。BT がそれを所有 / 駆動する system だから。禁止しているのは「**BT / Perception 以外の system** が Blackboard に依存して自動 join すること」。

## 22. 守るべき分離 (将来拡張しても維持)

| 役割 | 担当 |
|---|---|
| AI の意思決定 | `BehaviorTreeSystem` / `BehaviorTreeAsset` |
| 敵の検知 | `PerceptionSystem` / `PerceptionComponent` / `AggroComponent` |
| AI と StateMachine の橋渡し | `StateMachineParamsComponent` (BT が rising-edge で書き、SM が読む。§5.6 / §25) |
| AI の移動入力 | `LocomotionStateComponent.moveInput` を **world-space** で書く (§5.5)。`useCameraRelativeInput == false` 必須 |
| AI の状態保持 | `BehaviorTreeRuntimeComponent` (per-entity) |
| AI の知識ベース | `BlackboardComponent` (per-entity)。**読み書きは `BehaviorTreeSystem` と `PerceptionSystem` の 2 system のみ** |
| 敵の役割識別 | `EnemyTagComponent` |
| 敵の actor 識別 (GameLoop / 範囲攻撃) | `ActorTypeComponent::Enemy` |
| 敵の bundle | `EnemyConfigAsset` |
| 敵を生やす | `EnemyRuntimeSetup::SpawnFromConfig` |
| 敵を編集する | `PlayerEditorPanel` (ActorEditorMode = Enemy) |
| AI を編集する | `BehaviorTreeEditorPanel` |
| 攻撃系統 | `Attack` 1 系統に統一 (`AttackHeavy` を作らない)。Player 仕様 `PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md` §3 に従う |

## 23. 将来拡張 (v2.0+)

- Pathfinding (NavMesh) 統合の `MoveToPath` action
- Patrol waypoint プリセット (`WaypointComponent`)
- Squad / team coordination (shared blackboard)
- Stagger / posture (`PostureComponent` + posture-based 状態)
- Phase change による BT 切替 (low-HP rage 等)
- Custom action plugin (script から登録)
- Behavior Tree の profiling / cost view
- Replay (BT decision log)
- HTN / GOAP 系 planner ハイブリッド
- Async action (long-running navigation 等)
- BT の共有・継承 (asset reference + override)

## 24. リスクと対応

| リスク | 緩和策 |
|---|---|
| Tree が肥大化して maintain 困難 | sub-tree 機能 (Phase 6+) で分割 |
| Action 追加で type enum がパンクする | iota 連番でなくカテゴリ別に番号空間を分け、reserved 範囲を持つ (§4.2 で 100 単位刻み) |
| BlackboardValue の型不一致でクラッシュ | get で type 検証必須、不一致時は default 値返却 + warning log |
| `currentStateId` を BT が直接書く `PlayState` の濫用 | validate で warning、debug overlay で頻出を表示 |
| Player と Enemy が同じ asset を共有 | `EnemyTagComponent` 必須化で物理的に分離 |
| Tick 順依存で同一 frame 内の attack→damage が遅延 | §6 の system 順序を厳守、同 frame で attack が hit に届くこと |

## 25. 受信契約 (BT が書く先のフォーマット)

| 行先 | キー / フィールド | 型 | BT 側で書く action | 解釈モード | 読み手 |
|---|---|---|---|---|---|
| `LocomotionStateComponent.moveInput` | (XMFLOAT2) | float2 | MoveToTarget / Retreat / StrafeAroundTarget | **world x/z** (§5.5)。`useCameraRelativeInput == false` 前提 | `LocomotionSystem` |
| `LocomotionStateComponent.inputStrength` | (float) | float [0,1] | 同上 | スカラ | `LocomotionSystem` |
| `LocomotionStateComponent.targetAngleY` | (float) | rad | FaceTarget / MoveToTarget | world yaw | `LocomotionSystem` |
| `LocomotionStateComponent.useCameraRelativeInput` | (bool) | bool | (BT は触らない) | `EnemyRuntimeSetup` が `false` で初期化 | `LocomotionSystem` |
| `StateMachineParamsComponent.params` | `"Attack"` | float (1.0 / 0.0) | Attack (§5.6 rising-edge) | rising-edge trigger | `StateMachineSystem` |
| 同上 | `"Dodge"`  | float (1.0 / 0.0) | DodgeAction (§5.6 rising-edge) | rising-edge trigger | 同上 |
| 同上 | `"MoveStrength"` | float | MoveToTarget (連続値) | 連続 | 同上 |
| 同上 | (任意) | float | SetSMParam | (param による) | 同上 |
| `BlackboardComponent.entries` | 任意 | 任意 | SetBlackboard | per-entity map | 別 BT node / Perception |

各 1-frame trigger param (`Attack` / `Dodge`) は **BehaviorTreeSystem の冒頭で 0 にリセット** され、当該 frame で対応 action が phase 0 / 1 (要求送信中) の時のみ 1.0 を書く。phase 2 (in progress) の間は書かないため、StateMachine は **rising edge を 1 度のみ観測** する (§5.6)。

注: 「Light / Heavy」の 2 系統攻撃には対応しない。Player 仕様 (`PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md` §3) と揃えて Attack 1 系統で統一。コンボ表現は StateMachine 側の `comboCount` / Attack1→Attack2→Attack3 状態遷移で行う。

## 26. テスト

| カテゴリ | テスト |
|---|---|
| Unit | `BehaviorTreeAsset::SaveToFile` → `LoadFromFile` で同一性 |
| Unit | `Sequence` の Running 継続: 子 1 が Running 返したら次 frame は子 1 から |
| Unit | `Selector` の Running 継続: 上記と対称 |
| Unit | `Cooldown`: 直前 Success から t 秒未満は Failure |
| Unit | `Parallel.successThreshold = 2 / 3` 子で 2 子 Success → Success |
| Integration | Aggressive Template + Player 配置 → Enemy が Player を chase → 距離内で Attack → Player health が減る |
| Integration | `Attack` action の rising-edge: 距離内に居続けても **1 攻撃 = 1 サイクル**。動作中の `Attack` param は 0 / 1 / 0 / 0 / ... の 1 frame 立ち上がりで観測される (§5.6) |
| Integration | `MoveToTarget` 動作中、main camera を 180° 回転させても Enemy の進行方向が変わらない (camera 非依存、§5.5) |
| Unit       | `LocomotionSystem` が `useCameraRelativeInput == false` の entity に対し camera 変換をスキップする |
| Integration | Player を倒さず立ち止まり、5 sec 後に `loseTargetAfter` で aggro 解除 |
| Integration | Validate Error がある BT で Save 拒否 |
| Integration | mode 切替 (Player ↔ Enemy) で dock layout 再構築 + dirty 警告 |

## 27. 完了状態

Phase 1 〜 7 完了で v1.0 完成。Phase 6 (perception 拡張) は v1.0 では「sight 距離 + FOV のみ」で完了とし、raycast / hearing は v1.1 以降。

## 28. 関連 spec

- `GameLoop_SceneTransition_Spec_v1.1_2026-04-26.md` (Battle scene の遷移、`AllActorsDead Enemy` condition)
- `PlayerEditor_StateMachine_Ownership_Spec_2026-04-25.md` (StateMachine asset の所有関係)
- `PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md` (Player 用 RuntimeSetup の対称関係)
- `CinematicEditor_ECS_Sequencer_Spec_2026-04-09.md` (Timeline 連携)
