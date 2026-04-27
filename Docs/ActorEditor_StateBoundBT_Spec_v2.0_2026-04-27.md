# Actor Editor / State-bound AI-BT 統合仕様書 v2.0

作成日: 2026-04-27
v1.0 (`EnemyAI_BehaviorTree_Spec_v1.0_2026-04-27.md`) を全面改訂し、AI 編集を **PlayerEditor 内の StateMachinePanel に統合** する設計に変える。

## 0. 改訂理由

v1.0 で独立した `BehaviorTreeEditorPanel` を作ったが、運用上次の問題が判明した。

- **状態と AI の対応関係が見えない**: 「Attack state にいる時に何が起きるか」と「BT が何を tick しているか」を別 window で照合する必要があり、authoring 時の認知負荷が大きい。
- **2 つの top-level 構造が並走する**: StateMachine と BehaviorTree の双方が「現在の振る舞いの主導者」を主張する形になっていて、優先順序のルールが暗黙的になりやすい。
- **PlayerEditor との一貫性が低い**: Player は StateMachine 中心で authoring されているのに、Enemy は別 window という非対称。
- **ActorEditorMode (Player / Enemy / NPC) の入口がない**: 既存 PlayerEditor を Player 専用と決めうちしている。

これらを v2.0 で解消する。

## 1. 設計の中心方針 (確定)

1. **PlayerEditor を Actor Editor として再定義する** — toolbar に `ActorEditorMode` (Player / Enemy / NPC) を追加。
2. **AI / BT は StateMachine の各 State に紐づく (state-bound)** — 独立 BT 系として top-level tick しない。
3. **State-bound BT は state 滞在中だけ tick される** — state 退出で BT runtime を reset。state 復帰で activeStack を初期化。
4. **BT の役割は「state 内での挙動」と「state 退出条件のトリガ書込」** — SM transition を BT が直接呼ばず、`StateMachineParamsComponent` 経由で間接的に駆動する (v1.0 §5.6 の rising-edge 契約はそのまま継承)。
5. **既存 BehaviorTreeEditorPanel (独立窓) は撤去** — 機能は `StateMachinePanel` の State Inspector セクションに移植。
6. **既存 BehaviorTreeAsset / Node 型 / Validate / Tick セマンティクスは保持** — 「どこに置くか」「いつ動かすか」だけが変わる。
7. **EnemyConfigAsset / EnemyRuntimeSetup / PerceptionSystem は維持** — 役割不変。

## 2. ActorEditorMode (確定)

```cpp
enum class ActorEditorMode : uint8_t
{
    Player = 0,
    Enemy  = 1,
    NPC    = 2,
};
```

`PlayerEditorPanel` のメンバ:

```cpp
ActorEditorMode m_actorEditorMode = ActorEditorMode::Player;
```

### 2.1 Toolbar 表示

| 項目 | Player | Enemy | NPC |
|---|---|---|---|
| Setup Full Player | ✓ | ✗ | ✗ |
| Setup Full Enemy  | ✗ | ✓ | ✗ |
| Setup Full NPC    | ✗ | ✗ | ✓ |
| Repair Runtime    | ✓ (Player components) | ✓ (Enemy components) | ✓ (NPC components) |
| Save / Save As / Validate | 同 | 同 | 同 |

mode dropdown は toolbar 先頭に配置する。

### 2.2 切替時の挙動 (確定)

1. `m_actorEditorMode` 変更前に各 dirty (timeline / sm / socket / collider) があれば確認 dialog。Yes-Save / No-Discard / Cancel。
2. `m_actorEditorMode` 変更で `m_needsLayoutRebuild = true` を立てる (今 v1 の dock 構造では layout は基本不変だが将来拡張のために用意)。
3. preview entity の component を切替:
   - Player → Enemy: `PlayerTagComponent` 削除、`EnemyTagComponent` 付与、`EnsureAllEnemyRuntimeComponents`。
   - Enemy → Player: `EnemyTagComponent` 削除、`PlayerTagComponent` 付与、`EnsureAllPlayerRuntimeComponents`。
   - 任意 → NPC: 両 tag 削除、NPC 用 minimal setup (§7 参照)。

## 3. StateNode 拡張 (確定)

`StateMachineAsset.h` の `StateNode` に **2 フィールドを追加** する:

```cpp
struct StateNode
{
    uint32_t      id   = 0;
    std::string   name;
    StateNodeType type = StateNodeType::Locomotion;

    int      animationIndex = -1;
    uint32_t timelineId     = 0;
    bool     loopAnimation  = false;
    float    animSpeed      = 1.0f;
    bool     canInterrupt   = true;

    DirectX::XMFLOAT2 position { 0, 0 };
    std::unordered_map<std::string, float> properties;

    // ★ NEW: state 内で動かす BT (空なら BT なし = static state)。
    std::string behaviorTreePath;

    // ★ NEW: 任意のメモ書き (Inspector のラベル用、v1 ではメモのみ)。
    std::string aiNote;
};
```

挙動規約 (確定):

- `behaviorTreePath` が空: その state は AI 駆動なし。SM 既定挙動 (animation 再生のみ) で動作する。
- `behaviorTreePath` が空でない: その state に滞在中、対応 `.bt` を tick する。
- 1 entity の `BehaviorTreeRuntimeComponent` は **常に最大 1 本の BT** しか保持しない。state 切替で reset される (§5.4)。
- 同じ state を出入りするたびに BT は最初から動く (Cooldown / Repeat 等の途中状態は持ち越さない)。

## 4. StateMachineAsset シリアライズ (確定)

`StateMachineAssetSerializer.cpp` に追加 field を出力 / 入力する:

```json
{
  "states": [
    {
      "id": 1,
      "name": "Idle",
      "type": "Locomotion",
      "animationIndex": 0,
      "timelineId": 0,
      "behaviorTreePath": "Data/AI/BehaviorTrees/Knight_Idle.bt",
      "aiNote": "Detect & approach"
    }
  ]
}
```

- `behaviorTreePath` / `aiNote` は省略可能 (なければ空文字)。
- 既存 `.sm` ファイルは互換 (両 field 欠けたまま読込可能)。

## 5. Runtime: 実行モデル (確定)

### 5.1 BehaviorTreeAssetComponent の役割変更

| 旧 (v1.0) | 新 (v2.0) |
|---|---|
| Entity ごとに 1 本 top-level BT | (撤去 OR fallback) |

- v2.0 では **`BehaviorTreeAssetComponent` を削除しない**。互換性のため残し、**state-bound BT が空のときの fallback** として使う。
- `BehaviorTreeAssetComponent.assetPath` が設定されている entity は、現在の state の `behaviorTreePath` が空のときだけ fallback として tick される。
- 新 entity の標準は **state-bound のみ** 利用 (`BehaviorTreeAssetComponent.assetPath` は空)。

### 5.2 BehaviorTreeRuntimeComponent の追加 field

```cpp
struct BehaviorTreeRuntimeComponent
{
    // ... 既存フィールド省略

    // ★ NEW: 直近 tick 時の StateMachine state id。
    // 値が変わったら ResetAll() してから新 state の BT を tick する。
    uint32_t lastTickedStateId = 0;
};
```

### 5.3 BehaviorTreeSystem::Update (差替)

疑似コード (確定):

```text
for each Enemy entity (EnemyTag + BTRuntime + Blackboard + SMParams + StateMachineAssetComponent):
    currentState = SMParams.currentStateId
    if currentState != runtime.lastTickedStateId:
        runtime.ResetAll()
        runtime.lastTickedStateId = currentState

    btPath = ""
    stateNode = StateMachineAsset.FindState(currentState)
    if stateNode and not stateNode.behaviorTreePath.empty():
        btPath = stateNode.behaviorTreePath
    elif BehaviorTreeAssetComponent exists and not assetPath.empty():
        btPath = assetPath           # fallback for v1 compatibility

    if btPath.empty():
        continue                      # state has no AI; SM-only

    asset = LoadOrCache(btPath)
    if not asset: continue

    SMParams.SetParam("Attack", 0.0f)   # rising-edge reset (v1.0 §5.6 unchanged)
    SMParams.SetParam("Dodge",  0.0f)

    Tick(asset, asset.root, ctx)
```

### 5.4 State 切替時の reset (厳格定義)

state 切替で次のものを **必ず reset** する:

- `BehaviorTreeRuntimeComponent.activeNodeStack` / `activeNodeStackDepth = 0`
- `BehaviorTreeRuntimeComponent.nodeStateCount = 0` (Wait timer / Cooldown / Repeat / rising-edge phase 全消去)
- `BehaviorTreeRuntimeComponent.debugTraceCount = 0`
- `BehaviorTreeRuntimeComponent.lastTickedStateId = currentState`

reset しないもの (state 跨ぎ persistent):

- `BlackboardComponent.entries` (PerceptionSystem が管理)
- `AggroComponent` (PerceptionSystem が管理)

### 5.5 Frame phase 順 (v1.0 §6 を継承、変更なし)

```
PerceptionSystem
PlayerInputSystem        (Player tag のみ)
BehaviorTreeSystem       (Enemy tag のみ、state-bound)
PlaybackSystem
StateMachineSystem       (BT が書いた params で transition 評価)
LocomotionSystem
... 以下既存
```

順序ルール:

- BT → SM の順は維持。今 frame で BT が `Attack=1` を立て → 同 frame の SM が transition 評価。state 切替は次 frame の BT tick で reset 観測。
- これで "BT が新 state の入口で旧 BT 状態を引きずる" 問題は起きない。

## 6. StateMachinePanel 拡張 (Enemy / NPC mode のみ表示) (確定)

### 6.1 State Inspector に "AI / Behavior Tree" セクション追加

選択中 state がある時、Inspector に次のグループを表示:

```
┌─ State: <name> (id=<id>) ───────────┐
│ animationIndex [▾]                   │
│ timelineId     [   ]                 │
│ ...                                  │
├─ AI / Behavior Tree (Enemy/NPC mode) ┤
│ aiNote         [_______________]     │
│ btPath         [_______________]     │
│ [Browse...]    [New Default Subtree] │
│ [Edit Inline ▼]                      │
│ ┌─────────────────────────────────┐ │
│ │ Inline Tree View                 │ │
│ │  Root                            │ │
│ │   └─ Sequence                    │ │
│ │       ├─ TargetInRange (3.0)     │ │
│ │       └─ Attack                  │ │
│ │ Inspector for selected node ...  │ │
│ │ [Save] [Validate]                │ │
│ └─────────────────────────────────┘ │
└──────────────────────────────────────┘
```

実装 (確定):

- `m_actorEditorMode == Player` の場合は **このセクションを描画しない**。
- inline tree view は v1.0 `BehaviorTreeEditorPanel` の DrawTreeView / DrawNodeInspector を `StateMachinePanel` に流用 (同じコードを再利用、または共通 widget 化)。
- `[New Default Subtree]` は state type に応じた default サブツリーを生成:

| state type / 命名 | default subtree |
|---|---|
| state name に "idle" を含む | Selector { Sequence { HasTarget, SetSMParam("Attack", 1.0f) }, Wait(0.5) } |
| state name に "attack" を含む | (BT なし。アニメ再生で完結) |
| state name に "chase" を含む | Sequence { HasTarget, MoveToTarget(stopRange=1.5) } |
| state name に "damage" / "dead" を含む | (BT なし) |
| その他 | Wait(1.0) のみ |

- `[Save]` で `behaviorTreePath` の `.bt` ファイルに JSON 書出。`btPath` が未設定なら `Save As` ダイアログで path を求める。
- `[Validate]` で `ValidateBehaviorTree(asset)` を呼んで結果表示。

### 6.2 Player mode での挙動

- AI セクションは描画しない。
- 既存 Player の StateMachine 動作は変更なし。

## 7. Setup ボタン (確定)

### 7.1 Setup Full Enemy

新規 ボタン: `[Setup Full Enemy]` (Enemy mode のみ表示)。

押下時の動作 (確定):

1. preview entity に対し `EnemyRuntimeSetup::EnsureEnemyRuntimeComponents` 実行。
2. 既存の `Setup Full Player` が生成する Locomotion + Damage + Dead state は再利用、加えて以下:
   - `Idle` state (default。`behaviorTreePath = ""`、後で Setup が default subtree を生成して保存)
   - `Chase` state (`behaviorTreePath = "Data/AI/BehaviorTrees/<EnemyName>_Chase.bt"`)
   - `Attack1` state (既存 Player template と同じ。`behaviorTreePath = ""`)
   - 必要に応じて `Damaged` / `Dead`
3. `transitions` に必要なものを追加:
   - `Idle → Chase` (param: HasTarget = 1)
   - `Chase → Attack1` (param: TargetInRange < 2.0 ※ BT が SetSMParam で書く)
   - `Attack1 → Idle` (animFinished)
   - `Damaged → Idle` (animFinished)
4. 各 state について `behaviorTreePath` のファイルを default subtree で生成 (§6.1 表)。
5. `EnemyConfigAsset` を `Data/AI/Enemies/<Name>.enemy` に保存 (BT path 等)。

### 7.2 Setup Full NPC

NPC mode の `[Setup Full NPC]` は v2.0 では:

- `EnemyRuntimeSetup::EnsureEnemyRuntimeComponents` の **subset** を実行する `NPCRuntimeSetup` を新設 (perception / aggro なし)。
- `Idle` state のみ作成、`behaviorTreePath` は空。
- 必要に応じて将来 `Talk` / `Wander` state を追加可能。

NPC は ActorType::NPC タグを持ち、PerceptionSystem の対象外。

### 7.3 Repair Runtime

`[Repair Runtime Components]` を mode 別に表示:

| mode | 動作 |
|---|---|
| Player | `EnsureAllPlayerRuntimeComponents(registry, false)` 同等 |
| Enemy  | `EnsureAllEnemyRuntimeComponents(registry, false)` 同等 |
| NPC    | NPC 版 (Locomotion + ActionState + StateMachine のみ。AI 系なし) |

## 8. 撤去するもの (確定)

| 撤去対象 | 替わりの場所 |
|---|---|
| `Source/AI/BehaviorTreeEditorPanel.h` / `.cpp` | StateMachinePanel に inline tree view + inspector を移植 |
| `EditorLayer::m_showBehaviorTreeEditor` flag | (削除) |
| `EditorLayer::m_behaviorTreeEditorPanel` member | (削除) |
| `WindowFocusTarget::BehaviorTreeEditor` enum 値 | (削除) |
| `Window > Behavior Tree Editor` メニュー項目 | (削除) |

撤去しないもの:

- `BehaviorTreeAsset.h/cpp` (data 層)
- `BehaviorTreeSystem.h/cpp` (実行) — §5.3 で書き換え
- `BehaviorTreeRuntimeComponent.h` — §5.2 で field 追加
- `BlackboardComponent.h` / `PerceptionComponent.h` / `AggroComponent.h`
- `EnemyTagComponent.h` / `EnemyConfigAsset.h/cpp`
- `EnemyRuntimeSetup.h/cpp`
- `PerceptionSystem.h/cpp`
- `BehaviorTreeAssetComponent.h` (fallback として残す)

## 9. Validate (確定)

### 9.1 StateMachineAsset 用の追加 validate

| ID | 内容 | severity |
|---|---|---|
| SM-AI-1 | `behaviorTreePath` が空でない state に対して、ファイルが存在する | Warning |
| SM-AI-2 | 各 `behaviorTreePath` の `.bt` が `ValidateBehaviorTree` で Error なし | Warning (BT 自身の Error は最終的には Error 扱い) |
| SM-AI-3 | Enemy / NPC mode で **すべての state** が `behaviorTreePath` 空 + `BehaviorTreeAssetComponent.assetPath` も空 | Warning ("No AI defined") |

### 9.2 実行タイミング

- `[Validate]` ボタン (PlayerEditor toolbar) で全件
- `Save` 時に Error あれば abort
- `Play` 押下時、Enemy entity に対し state-bound BT を全件 validate (Error は警告のみ、Play 続行)

## 10. 受け入れ条件 (Phase 全完了時) (確定)

- [ ] PlayerEditor toolbar に `Mode: [Player ▼]` dropdown が表示され、Player / Enemy / NPC を切替できる
- [ ] Mode 切替時、dirty 警告が出る (no-discard 動作可能)
- [ ] Enemy mode で StateMachinePanel の State Inspector に "AI / Behavior Tree" セクションが現れる
- [ ] そのセクション内で `behaviorTreePath` を text 入力 / Browse / New Default Subtree で設定できる
- [ ] inline tree view + inspector で BT を編集できる ([Save] / [Validate] 動作)
- [ ] Player mode では AI セクションが描画されない
- [ ] `Setup Full Enemy` で entity に Enemy 一式 + StateMachine + 各 state の `.bt` 自動生成
- [ ] Play 押下時、entity が "Idle" state なら Idle の BT (HasTarget → SetSMParam("Attack", 1)) が tick される
- [ ] state が Idle → Attack1 に遷移したら、次 frame で BT runtime が reset され、Attack1 の BT (BT なし扱い) が tick されない
- [ ] state が Attack1 → Idle に戻ったら、再度 Idle の BT が **最初から** tick される (Wait timer 等が引き継がれない)
- [ ] BehaviorTreeEditorPanel の独立 window が menu に存在しない
- [ ] `Window` メニューに「Behavior Tree Editor」項目がない
- [ ] `EngineKernel` の `BehaviorTreeEditor` 関連 flag / member が消えている
- [ ] Debug ビルド成功 (`Game.exe` 生成、エラーゼロ)

## 11. 禁止事項 (確定)

- `BehaviorTreeSystem` で `StateMachineParamsComponent.currentStateId` を読まずに top-level tick すること
- state 切替で BT runtime を reset しないこと (Wait timer / Cooldown が引き継がれて挙動が壊れる)
- `BehaviorTreeAssetComponent.assetPath` が空でない場合に **state.behaviorTreePath を無視** すること (state-bound 優先、fallback 後)
- Player mode で AI セクションを描画すること
- `Setup Full Enemy` が **既存の Player runtime component を破壊** すること (preview entity が Player tag を持っているなら NoOp + warning)
- ActorEditorMode 切替で dirty 確認を出さずに asset を上書き / 破棄すること
- `BehaviorTreeEditorPanel` を再導入すること (機能は StateMachinePanel に統合済)
- `EnemyConfigAsset.behaviorTreePath` を **state-bound BT と独立** に解釈すること (用途を「fallback または default subtree の保存先」のいずれかに限定)
- 1 entity に複数の `.bt` を **同 frame で同時 tick** すること (1 entity 1 BT を厳守)

## 12. 守るべき分離 (将来拡張しても維持)

| 役割 | 担当 |
|---|---|
| state 単位の AI 振る舞い | StateNode + state.behaviorTreePath + BehaviorTreeAsset |
| state 切替の判定 | StateMachineSystem (params + transition で従来通り) |
| state 内の BT 駆動 | BehaviorTreeSystem (state-bound mode) |
| BT が書く先 | StateMachineParamsComponent / LocomotionStateComponent / Blackboard |
| 知覚 / target 取得 | PerceptionSystem |
| Authoring | PlayerEditor (Actor Editor) > StateMachinePanel > State Inspector AI セクション |
| Asset 形式 | `.sm` (state-bound パス含む) + `.bt` (subtree) |
| ActorTypeComponent | GameLoop / 範囲攻撃の actor 識別 (変更なし) |
| EnemyTagComponent | AI ルーティング (変更なし) |

## 13. Phase 計画 (実装順) (確定)

### Phase 1: data + serialize (StateNode 拡張)

完成条件:

- `StateNode` に `behaviorTreePath` / `aiNote` 追加
- `StateMachineAssetSerializer` で両 field を JSON 入出力 (省略時は空文字)
- 既存 `.sm` ファイルが読込でクラッシュしないこと

### Phase 2: BehaviorTreeRuntimeComponent + BehaviorTreeSystem 改修

完成条件:

- `BehaviorTreeRuntimeComponent.lastTickedStateId` 追加
- `BehaviorTreeSystem::Update` を §5.3 の疑似コードどおりに書換
- state 切替で BT runtime が reset されること
- `state.behaviorTreePath` が空、`BehaviorTreeAssetComponent.assetPath` が空でも crash しない

### Phase 3: PlayerEditor ActorEditorMode

完成条件:

- `ActorEditorMode` enum + member
- toolbar dropdown
- mode 切替時の dirty dialog (Save / Discard / Cancel)
- preview entity の component 切替 (Player tag ↔ Enemy tag)
- mode に応じた `Setup Full XXX` ボタンの出し分け

### Phase 4: StateMachinePanel に AI セクション統合

完成条件:

- State Inspector の AI セクション (Enemy / NPC mode のみ表示)
- `behaviorTreePath` 編集
- inline tree view + inspector (v1.0 BT panel コードを移植)
- `New Default Subtree` ボタン (§6.1 表)
- inline `[Save]` / `[Validate]`

### Phase 5: Setup Full Enemy / NPC

完成条件:

- `Setup Full Enemy` ボタン: §7.1 の動作
- `NPCRuntimeSetup` 新設 + `Setup Full NPC` ボタン
- `Repair Runtime Components` を mode 別に分岐

### Phase 6: BehaviorTreeEditorPanel 撤去

完成条件:

- `Source/AI/BehaviorTreeEditorPanel.h` / `.cpp` 削除 (Game.vcxproj / .filters からも除外)
- `EditorLayer.h` の `m_showBehaviorTreeEditor` / `m_behaviorTreeEditorPanel` / `WindowFocusTarget::BehaviorTreeEditor` 削除
- `EditorLayer.cpp` / `EditorLayerMenu.cpp` の関連箇所削除

### Phase 7: 統合確認

完成条件:

- §10 受け入れ条件全件成立
- Debug ビルドが clean (新規 warning ゼロ)

## 14. テスト (確定)

| カテゴリ | テスト |
|---|---|
| Unit | StateMachineAsset の JSON ラウンドトリップで `behaviorTreePath` / `aiNote` が保たれる |
| Unit | StateNode に空 path を保存 → 読込で空 path のまま |
| Unit | BehaviorTreeSystem: state 切替直後の frame で `lastTickedStateId` が更新され、`activeNodeStack` が空 |
| Unit | state.behaviorTreePath が空かつ `BehaviorTreeAssetComponent.assetPath` も空 → tick がスキップされる (crash しない) |
| Integration | Idle BT で `HasTarget` が成立 → SetSMParam("Attack", 1) → 次 frame で SM が Attack1 に遷移 → Attack1 終了で Idle に戻り、新 Idle BT が最初から tick される |
| Integration | Player mode の StateMachinePanel に AI セクションが現れない |
| Integration | Enemy mode に切替後、AI セクションが現れる |
| Integration | dirty 状態で mode 切替 → 確認 dialog |

## 15. 移行 (互換性) (確定)

v1.0 で作った Asset の互換動作:

- `BehaviorTreeAssetComponent.assetPath` を持つ既存 Enemy entity は **fallback** として動く。state.behaviorTreePath が空ならそれを使う。
- `Data/AI/BehaviorTrees/AggressiveKnight.bt` を直接参照するシーンも継続動作。
- v2.0 で Setup Full Enemy を実行した entity は state-bound 経路で動き、fallback は使われない。

## 16. 関連 spec

- `EnemyAI_BehaviorTree_Spec_v1.0_2026-04-27.md` (v1.0; v2.0 によって panel 部分は obsoleted)
- `PlayerEditor_StateMachine_Ownership_Spec_2026-04-25.md` (StateMachine 所有関係)
- `PlayerEditor_SetupFullPlayer_Spec_2026-04-25.md` (Player の Setup 仕様、Setup Full Enemy 実装の参照)
- `GameLoop_SceneTransition_Spec_v1.1_2026-04-26.md` (Battle scene 配置)
