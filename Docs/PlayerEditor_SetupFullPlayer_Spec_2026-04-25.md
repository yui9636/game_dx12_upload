# PlayerEditor SetupFullPlayer 仕様書

## 1. 目的

PlayerEditor の `Setup Full Player` ボタンを 1 つに集約し、押すだけで以下を含む完成 Player prefab を 1 度に生成できるようにする。

- Locomotion（Idle / Walk / Jog / Run）
- Attack コンボ（Attack1 / Attack2 / Attack3）
- Dodge
- Damage
- 上記すべてを駆動する InputMap / Parameter / ActionDatabase

`PlayerEditor_StateMachine_Input_Action_Authoring_Spec_2026-04-23.md` で定義した段階 Setup（Phase 1A / Phase 1B）の最終形を、本仕様書では単一プリセットとして定義する。

`PlayerEditor_StateMachine_Ownership_Spec_2026-04-25.md` の所有権ルール（StateMachine が source of truth）に従う。本仕様で生成した State / Transition / Parameter / InputMap / ActionDatabase は、すべて Ownership 仕様の枠内で動作する。

## 2. 現在の問題

現状、`Setup Full Player` ボタンは PlayerEditor 内に 3 箇所存在する。

- `PlayerEditorPanel.cpp:2118` ツールバー上のボタン
- `PlayerEditorPanel.cpp:2467` 空グラフ中央のボタン
- `PlayerEditorPanel.cpp:3327` パラメータパネル下のボタン

そのうち 2 つは `ApplyFullPlayerPhase1APreset()`（Idle / Move のみ）を呼び、1 つは `ApplyFullPlayerPreset()`（Idle / Move / Light1）を呼んでいる。

ボタン名が同じなのに生成内容が異なるため、エディタ UI として不整合。

さらに、最強プリセットでも生成されるのは Idle / Move / Light1 のみで、結果として GameLayer 上では「Walk アニメーション 1 個を再生したまま速度だけ変わる」状態になる。

ユーザー視点では「ゆっくり歩くだけ」に見える。

これは仕様として不十分。

## 3. 最重要方針

`Setup Full Player` を、エディタの「これを押せば完成形が出る」唯一のボタンとして定義する。

- ボタンは 1 つに集約する。重複箇所はメインボタンと同じプリセットを呼ぶ。
- 生成内容は v1 完成条件（Locomotion + Attack コンボ + Dodge + Damage）を満たす。
- 攻撃は Light / Heavy の 2 系統に分けず、`Attack` 1 系統で統一する。エディタ UI として攻撃概念を単純化する。
- Walk / Jog / Run は `Gait` パラメータの値（1 / 2 / 3）で State を切り替える。
- Damage は外部からの被弾要求に応じて `Damaged` トリガーで遷移する。
- ActionDatabase は `Setup Full Player` と同時に Attack 用 payload を初期化する。

### 3.1 Gait 更新タイミング

`Gait` parameter の真値は `LocomotionSystem` が更新した `LocomotionStateComponent.gaitIndex` である。`StateMachineSystem` はそれを `StateMachineParamsComponent.Gait` parameter として mirror した上で transition 評価する。

GameLayer の更新順は `PlayerEditor_StateMachine_Ownership_Spec_2026-04-25.md` §6 に従い `StateMachineSystem` -> `LocomotionSystem` の順で確定している。

つまり `StateMachineSystem` が読む `Gait` は **前フレームの `LocomotionSystem` 結果** である。これは設計上の 1 frame 遅延を伴う仕様であり、許容する。

迷いを排除するための確定事項。

- `LocomotionSystem` を `StateMachineSystem` の前に移動してはいけない。Ownership §6 と矛盾する。
- `LocomotionSystem` 内で `StateMachineParamsComponent.SetParam("Gait", ...)` を呼んではいけない。Param mirror の二重実装になる。
- `Gait` の Param mirror 経路は `StateMachineSystem` の `SyncLocomotionParameters`（[StateMachineSystem.cpp:130-152](Source/Gameplay/StateMachineSystem.cpp:130)）唯一とする。
- 1 frame 遅延を許容できない判定（例: 即時キャンセル）は `Gait` ではなく `IsMoving` などの別 parameter / 別 trigger で表現する。
- Walk / Jog / Run の State 切替は 1 frame 遅延しても見た目に支障が出ない。これは v1 として許容する。

### 3.2 Trigger 消費ルール

`Attack` / `Dodge` / `Damaged` の各 trigger parameter は **1 frame だけ `1.0f` になり、その frame の `StateMachineSystem` 評価後に必ず `0.0f` に戻る**。

- 立てる側
    - `Attack` / `Dodge` は `PlayerInputSystem` が `ResolvedInputStateComponent.actions[idx].pressed` の edge を検出した frame だけ `1.0f` を書く（[PlayerInputSystem.cpp:67-69](Source/Gameplay/PlayerInputSystem.cpp:67)）。
    - `Damaged` は `HealthSystem`（または被弾検知側）が被弾を検出した frame だけ `1.0f` を書く。
- 消費する側
    - `StateMachineSystem` が transition 評価後に `0.0f` に戻す（[StateMachineSystem.cpp:260-265 `ClearTriggerParameters`](Source/Gameplay/StateMachineSystem.cpp:260)）。
    - transition が成立してもしなくても、その frame の StateMachine 評価後には必ず `0.0f` に戻す。次 frame に繰り越さない。

迷いを排除するための確定事項。

- trigger を「pressed の継続状態」として扱ってはいけない。ボタンを押し続けても trigger は 1 frame しか立たない。次の Attack を発火するには一度離して再度押す。
- `StateMachineSystem` 以外で trigger を `0.0f` に戻してはいけない。`PlayerInputSystem` は `1.0f` を書くだけ、`HealthSystem` も `1.0f` を書くだけ。
- `StateMachineSystem` が早期 `continue` する経路（asset が空、現在 state が見つからない、評価対象 transition 無し、など）でも `ClearTriggerParameters` は必ず通す。立ったまま次 frame へ繰り越す経路を作らない。
- `ClearTriggerParameters` が消費する trigger は本仕様時点で `Attack` / `Dodge` / `Damaged` の 3 種。新規 trigger を追加する場合は同関数にも追記する。
- 既存 prefab に残っている `LightAttack` / `HeavyAttack` parameter は `ClearTriggerParameters` の対象から外してよい。`Setup Full Player` 後は使われなくなる。

### 3.3 Setup Full Player 重複生成禁止

`Setup Full Player` は **何回押しても** 生成物の数が増えない。

- すべての要素は「既存を名前 / index で検索 -> 見つかれば再利用 -> 無ければ追加」のパターンで実装する。
- ユーザーが State 名や Transition の `from/to` を変更した場合、それは別物として扱う。再度 `Setup Full Player` を押すと不足分が補完されるが、改名済みの既存要素は残る。

迷いを排除するための確定事項。

- `m_stateMachineAsset.AddState(...)` を無条件に呼んではいけない。`FindStateByName(...)` で先に探す（既存の `ApplyLocomotionStateMachinePreset` がこのパターン）。
- `m_stateMachineAsset.AddTransition(from, to)` を無条件に呼んではいけない。`FindTransition(from, to)` で先に探す。
- 同じ `from` / `to` ペアの transition は 1 本までとする。条件違いで複数本作りたい場合は別途仕様化する（v1 では作らない）。
- `EnsureStateMachineParameter(name, type, default)` は内部で名前検索済みなので無条件に呼んでよい。
- `EnsureActionBinding` / `EnsureAxisBinding`（[PlayerRuntimeSetup.cpp:95-152](Source/Gameplay/PlayerRuntimeSetup.cpp:95)）は内部で名前検索済みなので無条件に呼んでよい。
- `ActionDatabaseComponent` の node は `index` で識別する。`Setup Full Player` 実行時に Attack1〜3 が index 0〜2 に存在することを確認し、無ければ追加する。あればユーザー編集値を温存する。
- `animationIndex` の再割当ては以下の条件のときのみ行う。それ以外はユーザー設定を尊重する。
    - `animationIndex < 0`（未設定）
    - 現在割り当てられているアニメ名が方向違い locomotion（`back` / `left` / `right` / `strafe` / `_l45` / `_r45` / `_l90` / `_r90` / `turn_l` / `turn_r` を含む）
- `StateNode.properties["ActionNodeIndex"]` は無条件に上書きしてよい（StateName と ActionDatabase index の対応は固定なので、ユーザーが手動で変える理由がない）。
- `Setup Full Player` 2 回押下後、StateMachine の State 数 / Transition 数 / Parameter 数 / InputMap action 数 / InputMap axis 数 / ActionDatabase node 数 はすべて 1 回押下後と同一であること。これは §17 受け入れ条件にも反映する。

## 4. 禁止事項

以下は禁止。

- 同名ボタン `Setup Full Player` から異なるプリセットを呼ぶ。
- `Setup Full Player` で State だけを作って InputMap / Parameter / ActionDatabase を生成しない。
- StateMachine に Attack State が無いまま ActionDatabase に Attack ノードを残す。
- Heavy / Light を別 trigger に分けて Action State を二重化する。
- Walk / Jog / Run を 1 つの Move State + animation 自動切替で誤魔化す（v1 では State を分ける）。
- Damage / Dodge を Phase 後送りにして本ボタンで作らない。

## 5. ボタン整理

`PlayerEditorPanel.cpp` 内 3 箇所の `Setup Full Player` ボタンを以下のように整理する。

### 5.1 残す箇所

- `DrawStateMachinePanel` ツールバー（line 2118）。メインボタン。
- `DrawNodeGraph` 空グラフ中央（line 2467）。チュートリアル代わり。

### 5.2 削除する箇所

- `DrawStateMachineParameterList`（line 3327）の `Setup Full Player`。`Add Attack` ボタンで Attack だけ追加できるよう置き換える。

### 5.3 ポップアップメニュー整理

`AddStateTemplatePopup`（line 2147 周辺）と `GraphBgCtx`（line 2447 周辺）の段階別プリセット項目は次の通り整理する。

- `Setup Full Player`（v1 完成 = 本仕様）
- `Setup Locomotion Only`（Idle / Walk / Jog / Run のみ）
- `Add Attack Combo`（Attack1〜3 + ActionDatabase）
- `Add Dodge`（Dodge State + Dodge InputMap）
- `Add Damage`（Damage State + Damaged トリガー）

State 単体追加（Locomotion / Action / Dodge / Damage / Custom）は今まで通りメニューに残す。

## 6. 生成する State

`Setup Full Player` は以下 9 State を生成する。

### Locomotion 系（Type: Locomotion, loop = true）

| 名前 | アニメ候補（小文字一致） | animSpeed |
|---|---|---|
| `Idle` | `idle` | 1.0 |
| `Walk` | `walk_front` / `walk_f` / `walk` | 1.0 |
| `Jog`  | `jogging_f` / `jog_f` / `jog`  | 1.0 |
| `Run`  | `run`（ただし `run_fast` / `run_injured` は除外） | 1.0 |

すべて `back` / `left` / `right` / `strafe` / `_l45` / `_r45` / `_l90` / `_r90` / `turn_l` / `turn_r` を含むアニメは候補から除外する。

### Action 系（Type: Action, loop = false）

| 名前 | アニメ候補 | ActionNodeIndex |
|---|---|---|
| `Attack1` | `combo1` / `combo_1` / `attack1` | 0 |
| `Attack2` | `combo2` / `combo_2` / `attack2` | 1 |
| `Attack3` | `combo3` / `combo_3` / `attack3` | 2 |

State の `properties["ActionNodeIndex"]` で `ActionDatabase` の対応 node を参照する。

### Dodge 系（Type: Dodge, loop = false）

| 名前 | アニメ候補 |
|---|---|
| `Dodge` | `dodge_front` / `dodge_f` / `dodge` |

### Damage 系（Type: Damage, loop = false）

| 名前 | アニメ候補 |
|---|---|
| `Damage` | `damage_front_small` / `damage_f` / `damage` |

## 7. 生成する Transition

合計 26 本。priority は値が大きいほど優先される。

### 7.1 Locomotion 内遷移（gait 切替）

- `Idle -> Walk`: `Gait >= 1` (priority 100)
- `Walk -> Idle`: `Gait <= 0` (priority 100)
- `Walk -> Jog`:  `Gait >= 2` (priority 100)
- `Jog -> Walk`:  `Gait <= 1` (priority 100)
- `Jog -> Run`:   `Gait >= 3` (priority 100)
- `Run -> Jog`:   `Gait <= 2` (priority 100)

### 7.2 Locomotion -> Action（4 本）

- `Idle -> Attack1`: `Attack == 1` (priority 200)
- `Walk -> Attack1`: `Attack == 1` (priority 200)
- `Jog  -> Attack1`: `Attack == 1` (priority 200)
- `Run  -> Attack1`: `Attack == 1` (priority 200)

### 7.3 Locomotion -> Dodge（4 本）

- `Idle -> Dodge`: `Dodge == 1` (priority 300)
- `Walk -> Dodge`: `Dodge == 1` (priority 300)
- `Jog  -> Dodge`: `Dodge == 1` (priority 300)
- `Run  -> Dodge`: `Dodge == 1` (priority 300)

### 7.4 Locomotion / Action -> Damage（7 本）

- `Idle    -> Damage`: `Damaged == 1` (priority 500)
- `Walk    -> Damage`: `Damaged == 1` (priority 500)
- `Jog     -> Damage`: `Damaged == 1` (priority 500)
- `Run     -> Damage`: `Damaged == 1` (priority 500)
- `Attack1 -> Damage`: `Damaged == 1` (priority 500)
- `Attack2 -> Damage`: `Damaged == 1` (priority 500)
- `Attack3 -> Damage`: `Damaged == 1` (priority 500)

Dodge State からは Damage に遷移しない。回避中は無敵扱いとする。

### 7.5 Action コンボ（2 本）

- `Attack1 -> Attack2`: `Attack == 1` + `hasExitTime` + `exitTimeNormalized >= 0.4` (priority 200)
- `Attack2 -> Attack3`: `Attack == 1` + `hasExitTime` + `exitTimeNormalized >= 0.4` (priority 200)

`Attack3 -> Attack1` のループは作らない。

### 7.6 Action -> Dodge キャンセル（3 本）

- `Attack1 -> Dodge`: `Dodge == 1` + `hasExitTime` + `exitTimeNormalized >= 0.2` (priority 300)
- `Attack2 -> Dodge`: `Dodge == 1` + `hasExitTime` + `exitTimeNormalized >= 0.2` (priority 300)
- `Attack3 -> Dodge`: `Dodge == 1` + `hasExitTime` + `exitTimeNormalized >= 0.2` (priority 300)

### 7.7 終了遷移（5 本）

- `Attack1 -> Idle`: `AnimEnd == 1` (priority 100)
- `Attack2 -> Idle`: `AnimEnd == 1` (priority 100)
- `Attack3 -> Idle`: `AnimEnd == 1` (priority 100)
- `Dodge   -> Idle`: `AnimEnd == 1` (priority 100)
- `Damage  -> Idle`: `AnimEnd == 1` (priority 100)

## 8. 生成する Parameter

`StateMachineAssetComponent.parameters` に以下を確保する。

| 名前 | 型 | 既定値 | 用途 |
|---|---|---|---|
| `MoveX` | Float | 0.0 | LocomotionSystem -> mirror |
| `MoveY` | Float | 0.0 | LocomotionSystem -> mirror |
| `MoveMagnitude` | Float | 0.0 | LocomotionSystem -> mirror |
| `IsMoving` | Bool | 0.0 | LocomotionSystem -> mirror |
| `Gait` | Int | 0 | LocomotionSystem -> mirror |
| `IsWalking` | Bool | 0.0 | LocomotionSystem -> mirror |
| `IsRunning` | Bool | 0.0 | LocomotionSystem -> mirror |
| `Attack` | Trigger | 0.0 | PlayerInputSystem が 1 frame 立てる |
| `Dodge` | Trigger | 0.0 | PlayerInputSystem が 1 frame 立てる |
| `Damaged` | Trigger | 0.0 | HealthSystem 等が 1 frame 立てる |

`LightAttack` / `HeavyAttack` は廃止する。既存 prefab から検出された場合は参照を `Attack` へ寄せる。

## 9. 生成する InputMap

`InputActionMapComponent.asset` に以下を確保する。

### Axis

- `MoveX`: `D` / `A`, GamepadLeftStickX
- `MoveY`: `W` / `S`, GamepadLeftStickY

### Action

- `Attack`: `J` / MouseLeft / GamepadX
- `Dodge`: `Space` / GamepadB

`HeavyAttack` action は生成しない。

## 10. PlayerInputSystem の InputAction 番号

`PlayerInputSystem.cpp` の `namespace InputAction` を以下に変更する。

```cpp
namespace InputAction {
    constexpr int Attack = 0;
    constexpr int Dodge  = 1;
}
```

`AttackLight` / `AttackHeavy` 定義は削除する。

`HeavyAttack` parameter への書き込みは削除する。`Attack` と `Dodge` のみ trigger として書く。

## 11. ActionDatabase

`Setup Full Player` 実行時、`ActionDatabaseComponent` に Attack1〜3 用 ノードを生成する。

| index | name | comboStart | cancelStart | inputStart | inputEnd | animSpeed | damageVal |
|---|---|---|---|---|---|---|---|
| 0 | Attack1 | 0.4 | 0.2 | 0.0 | 1.0 | 1.0 | 10 |
| 1 | Attack2 | 0.4 | 0.2 | 0.0 | 1.0 | 1.0 | 12 |
| 2 | Attack3 | 0.5 | 0.3 | 0.0 | 1.0 | 1.0 | 18 |

`animIndex` は §6 の Action 系 State の animation と同一にする。

`nextLight` / `nextHeavy` フィールドは v1 では使わない（StateMachine が遷移を持つ）。互換のため残してよいが、`-1` のままにする。

## 12. アニメーション自動マッチング

各 State の `animationIndex` 解決は、PlayerEditorPanel に以下のヘルパーを追加して個別判定する。

- `FindIdleAnimation()`
- `FindWalkAnimation()`
- `FindJogAnimation()`
- `FindRunAnimation()`
- `FindAttackAnimation(int slot)` （slot は 1〜3）
- `FindDodgeAnimation()`
- `FindDamageAnimation()`

候補ルールは §6 の表に従う。

候補が見つからない場合の扱い。

- Idle / Walk / Jog / Run のいずれかが見つからない場合、`Setup Full Player` は警告を出すが State は生成する。`animationIndex = -1` のまま保存し、ユーザーが UI から手動で選び直せるようにする。
- Attack / Dodge / Damage が見つからない場合も同様。State は作る。

候補のマッチは `ToLowerAscii` 後の部分一致。除外キーワード（`back`, `left`, `right`, `strafe`, `_l45`, `_r45`, `_l90`, `_r90`, `turn_l`, `turn_r`）に該当するアニメは候補から除く。

## 13. PlayerRuntimeSetup の連動修正

`PlayerRuntimeSetup.cpp` を以下のように修正する。

### 13.1 EnsureDefaultPlayerInputMap

- `LightAttack` 生成を `Attack` に置換。
- `Dodge` action を生成し、Space キー / GamepadB を割り当てる。
- `HeavyAttack` 生成は行わない（既存実装にもないが、念のため）。

### 13.2 EnsureDefaultActionDatabase

- 既存の Light1 単発 1 ノード生成を、Attack1〜3 の 3 ノード生成に拡張する。
- 各ノードの `comboStart` / `cancelStart` / `damageVal` は §11 の表に従う。
- `animIndex` は `FindAttackAnimation(slot)` で解決する。

### 13.3 ResetLocomotionStateMachineParams

- `LightAttack` / `HeavyAttack` への `SetParam` を削除。
- `Attack` / `Dodge` / `Damaged` の `SetParam(0.0f)` を追加。

### 13.4 EnsureLocomotionRuntimeTuning

- 既存値を維持する。`walkMaxSpeed` / `jogMaxSpeed` / `runMaxSpeed` / `acceleration` などはユーザーが PlayerEditor 上から個別調整できる。

## 14. Damage State の取り扱い

Damage State 用 `Damaged` トリガーは、本仕様書ではトリガー parameter の存在と遷移配線のみを定義する。

実際のヒット検知から `Damaged = 1.0f` を立てる側の実装は別仕様（`HealthSystem` 拡張）に委ねる。

`Damage -> Idle` は `AnimEnd` で戻る。Damage アニメが loop = false である前提に依存する。

無敵時間が必要な場合は `HealthComponent.invincibleTimer` を使う。Damage State 自体には無敵処理を持たせない。

## 15. Editor UI 要件

`Setup Full Player` 実行後、以下が PlayerEditor 上で確認できること。

- StateMachine パネルに 9 個の State が並ぶ。
- 各 State のアニメ名がノードに表示される。
- パラメータパネルに `Attack` / `Dodge` / `Damaged` トリガーが見える。
- InputMap タブに `Attack` / `Dodge` action が見える（`LightAttack` / `HeavyAttack` は無い）。
- ActionDatabase に Attack1〜3 の 3 ノードが見える。

実行中のランタイム可視化は `DrawStateMachineRuntimeStatus` を流用する。新規追加すべき表示項目。

- `Damaged` トリガーの現値
- 現在 Action State のうち、参照 ActionNodeIndex

## 16. 実装フェーズ

### Phase α: ボタン整理

- `PlayerEditorPanel.cpp:3327` の `Setup Full Player` を削除。
- `PlayerEditorPanel.cpp:2118` と `:2467` を新 `ApplyFullPlayerPreset()` 呼び出しに統一。
- `AddStateTemplatePopup` / `GraphBgCtx` のメニューを §5.3 に従い再構成。

完成条件:
- ボタンを押す箇所によらず同じ生成内容になる。
- 削除した箇所が UI から消えている。

### Phase β: Parameter / InputMap 統一

- `LightAttack` / `HeavyAttack` 廃止、`Attack` / `Dodge` / `Damaged` 追加。
- `PlayerInputSystem.cpp` の `InputAction` 番号変更。
- `PlayerRuntimeSetup.cpp` の InputMap / ResetParams 連動修正。

完成条件:
- 既存 prefab を読んでも警告止まりで動く（自動 migration は不要）。
- `J` 押下で `Attack` trigger が 1 frame 立つ。
- `Space` 押下で `Dodge` trigger が 1 frame 立つ。

### Phase γ: State / Transition 拡張

- `ApplyLocomotionStateMachinePreset` を拡張し、Idle / Walk / Jog / Run の 4 State 生成へ変更。
- `ApplyAttackComboPreset` を新設し、Attack1〜3 + コンボ遷移を生成。
- `ApplyDodgePreset` を新設し、Dodge State + 4 本の Locomotion -> Dodge + 3 本の Action -> Dodge + Dodge -> Idle を生成。
- `ApplyDamagePreset` を新設し、Damage State + 7 本の Locomotion/Action -> Damage + Damage -> Idle を生成。
- `ApplyFullPlayerPreset` をこれら 4 つを順に呼ぶ実装に書き換える。

完成条件:
- `Setup Full Player` 1 回で 9 State / 26 Transition が生成される。
- WASD で Walk / Jog / Run が見た目で切り替わる（gait 増加に応じて animation も変わる）。
- `J` 連打で Attack1 -> Attack2 -> Attack3 -> Idle のコンボが繋がる。
- `Space` で任意の Locomotion / Action から Dodge へ遷移する。

### Phase δ: ActionDatabase 拡張

- `EnsureDefaultActionDatabase` を Attack1〜3 生成に拡張。
- 各 Action State の `properties["ActionNodeIndex"]` を 0 / 1 / 2 に設定する。

完成条件:
- ActionDatabase に Attack1〜3 が見える。
- StateMachine が Attack State を enter したとき、対応 ActionNode index が `ActionStateComponent.currentNodeIndex` に同期される（既存 `SyncActionStateFromStateNode` の動作）。

### Phase ε: アニメーション自動マッチング細分化

- `FindIdleAnimation` / `FindWalkAnimation` / `FindJogAnimation` / `FindRunAnimation` / `FindAttackAnimation` / `FindDodgeAnimation` / `FindDamageAnimation` を実装。
- 既存 `findPreferredMoveAnimation` は `FindWalkAnimation` で置換。

完成条件:
- A5.gltf を開いて `Setup Full Player` を押すと、Idle / Walk_Front / Jogging_F / Run / Combo1 / Combo2 / Combo3 / Dodge_Front / Damage_Front_Small が自動で割り当たる。

## 17. 受け入れ条件

- `Setup Full Player` を 1 回押すと、StateMachine に 9 State が並ぶ。
- 同じ操作で InputMap に `Attack` / `Dodge` が並ぶ。
- 同じ操作で ActionDatabase に Attack1〜3 が並ぶ。
- WASD 押し方の強弱（または LeftCtrl / LeftShift）で Walk / Jog / Run が見た目で切り替わる。
- `J` 押下で Attack1 が再生され、終了後 Idle に戻る。
- `J` を Attack1 中（exitTime >= 0.4）で再押下すると Attack2、さらに同じく Attack3 へ繋がる。
- `Space` 押下で Dodge が再生され、終了後 Idle に戻る。
- `Damaged` トリガーを外部から立てると Damage State へ遷移し、終了後 Idle に戻る。
- prefab 保存 -> LevelEditor 配置 -> Play で同じ挙動が再現される。
- `Setup Full Player` ボタンは PlayerEditor 内に最大 2 箇所（ツールバー / 空グラフ中央）しか存在しない。両者は同一プリセットを呼ぶ。
- `Setup Full Player` を 2 回連続で押下しても、StateMachine の State 数 / Transition 数 / Parameter 数 / InputMap action 数 / InputMap axis 数 / ActionDatabase node 数 はすべて 1 回押下後と同一である（§3.3 重複生成禁止）。
- `Attack` / `Dodge` / `Damaged` の各 trigger を立てた次 frame で、`StateMachineParamsComponent` 上の値が `0.0f` に戻っている（§3.2 Trigger 消費ルール）。
- `Gait` parameter が `LocomotionStateComponent.gaitIndex` の前 frame 値と一致している（§3.1 Gait 更新タイミング）。1 frame 遅延は許容。

## 18. 結論

`Setup Full Player` は、PlayerEditor が「これを押せば触れるキャラが出る」唯一のボタンとして再定義される。

攻撃は Light / Heavy を統合した `Attack` 1 系統に簡素化し、エディタ UI 上の概念を減らす。

Walk / Jog / Run / Attack コンボ / Dodge / Damage を含む 9 State がワンクリックで生成され、prefab 保存後の GameLayer 再生でも同じ挙動が再現される状態を v1 完成とする。
