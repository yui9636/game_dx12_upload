# PlayerEditor StateMachine / Input / Action Authoring 仕様書

## 1. 目的

PlayerEditor で、モデルを開いた直後に `Setup Full Player` を押すだけで、最低限ゲーム内で操作できる Player prefab を作れるようにする。

最初の成功ラインは `Idle / Move` だけに絞る。
攻撃、コンボ、Timeline イベントは次段階で積む。

## 2. 最重要方針

- 外部 JSON へ戻さない。StateMachine / InputMap / Timeline / ActionDatabase は prefab 内 component に保存する。
- UI だけ作って runtime が動かない状態を禁止する。
- Editor 専用の偽 state 切替は禁止する。
- 既存の `PlayerRuntimeSetup` / `InputResolveSystem` / `PlayerInputSystem` / `LocomotionSystem` / `StateMachineSystem` / `ActionSystem` を接続する。
- `std::min` / `std::max` 前提の新規実装は避け、既存流儀に合わせる。

## 3. Phase 1A: Locomotion 最小完成

Phase 1A は攻撃を入れない。
まず「保存した prefab を LevelEditor に置いて Play したら Idle / Move が動く」を完成条件にする。

### 生成する component

- `InputActionMapComponent`
- `InputBindingComponent`
- `InputContextComponent`
- `InputUserComponent`
- `StateMachineAssetComponent`
- `StateMachineParamsComponent`
- `ActionStateComponent`
- `LocomotionStateComponent`
- `PlaybackComponent`
- `AnimatorComponent`

### 生成する Input

- Axis `MoveX`: A/D, LeftStickX
- Axis `MoveY`: S/W, LeftStickY

### 生成する StateMachine

- `Idle`
- `Move`

### 生成する parameter 定義

- `MoveX`
- `MoveY`
- `MoveMagnitude`
- `IsMoving`
- `Gait`
- `IsWalking`
- `IsRunning`

### 遷移

- `Idle -> Move`: `IsMoving >= 1`
- `Move -> Idle`: `IsMoving <= 0`

## 4. Phase 1B: Light1 単発攻撃

Phase 1B で初めて攻撃を入れる。
コンボはまだ入れない。

### 追加 component

- `ActionDatabaseComponent`
- 必要なら `TimelineLibraryComponent`
- 必要なら `ColliderComponent`

### 追加 Input

- Action `LightAttack`: MouseLeft / J / Gamepad X

### 完成条件

- `LightAttack` で `Light1` animation に切り替わる。
- 攻撃中は `Attack` state 表示になる。
- 攻撃終了後に `Idle / Move` へ戻る。
- ここでは `Light2 / Light3 / Heavy1 / RunLight` は作らない。

## 5. Phase 2: Combo / Heavy / RunLight

Phase 2 でコンボを入れる。
UI はカード式にし、`nodeIndex` は見せない。

### Action card

- `Light1`
- `Light2`
- `Light3`
- `Heavy1`
- `RunLight`

### 設定項目

- Animation
- Timeline
- Input Window Start
- Input Window End
- Combo Commit Time
- Next Light
- Next Heavy
- Damage
- Anim Speed

### 初期リンク

- `Light1 -> Light2 -> Light3`
- `Light1 -> Heavy1`
- `Light2 -> Heavy1`
- Run 中 `LightAttack` で `RunLight`

## 6. Setup Full Player 仕様

`Setup Full Player` は段階実装する。
最初から全部を同時に成功条件にしない。

### Phase 1A で必須

- InputMap default 生成
- Idle / Move state 生成
- locomotion parameter 定義生成
- runtime component 補完
- prefab 保存
- scene 配置後 Play で Idle / Move

### Phase 1B 以降

- Light1 単発攻撃
- ActionDatabase 初期化
- TimelineLibrary 初期化
- Collider 初期化
- Combo card 初期化

## 7. InputMap の責務

`InputActionMapComponent` は Player prefab の persistent authoring data とする。

不足時に補完する経路。

- `Setup Full Player`
- `PrefabSystem::InstantiatePrefab()`
- `Gameplay > Player` 生成
- PlayerEditor preview entity 構築
- scene load 後の Player runtime repair

保存するもの。

- action 名
- axis 名
- keyboard bind
- mouse bind
- gamepad bind
- deadzone
- sensitivity

保存しないもの。

- current pressed
- current held
- current released
- frame counter
- resolved input runtime 値

## 8. Input UI 仕様

現行の scancode 表編集は v1 UI としては失敗。
PlayerEditor では UE 風に「Action 名 + Capture」へ寄せる。

### Movement

- Move X
- Move Y
- Walk Modifier
- Run Modifier

### Actions

- Light Attack
- Heavy Attack
- Dodge

### UI ルール

- scancode 数値を直接編集させない。
- action index を見せない。
- `Capture` ボタンで次に押したキー/ボタンを割り当てる。
- `Reset Defaults` で標準入力へ戻す。
- `Repair Player Input` で不足 action / axis を補う。

## 9. Input runtime 修正仕様

現状の `InputResolveSystem.cpp` は axis が event frame に寄りすぎる危険がある。
WASD 押しっぱなしで継続入力にならないと locomotion は完成しない。

必須修正。

- key down 状態を frame をまたいで保持する。
- axis は現在押されているキー状態から毎フレーム再計算する。
- gamepad axis は最後の値を保持する。
- deadzone を適用する。
- `IsMoving` は `MoveMagnitude` から決める。
- deadzone 付近で `Idle <-> Move` が震えないよう hysteresis を入れる。

禁止。

- `PlayerInputSystem` で ImGui 入力を見る。
- Preview 専用 input 分岐を作る。
- keydown event の瞬間だけ axis を 1 にする。

## 10. StateMachineParamsComponent の責務

parameter 定義と runtime 値更新を分ける。

### authoring

- parameter 定義は prefab に保存する。
- `Setup Full Player` が必須 parameter を作る。
- `Repair Runtime Components` は不足 parameter だけ追加する。

### runtime

- runtime system は parameter 定義を変更しない。
- runtime system は値だけ更新する。
- `StateMachineSystem` は parameter を読んで条件評価する。

## 11. Locomotion の source of truth

locomotion 系 parameter の source of truth は `LocomotionStateComponent` と `LocomotionSystem` の計算結果。
`StateMachineParamsComponent` は StateMachine 条件評価用の mirror 先。

禁止。

- `StateMachineSystem` で移動量を再計算する。
- `StateMachineSystem` 側で gait 判定を重複実装する。

## 12. Walk / Jog / Run 方針

v1 標準は「単一 MoveClip + 回転 + 速度差」。
Walk / Jog / Run の専用 animation は任意。

### 標準

- IdleClip
- MoveClip
- WalkSpeed
- JogSpeed
- RunSpeed
- WalkAnimSpeed
- JogAnimSpeed
- RunAnimSpeed

### 入力による gait

- Gamepad: stick magnitude で Walk / Jog / Run
- Keyboard WASD: Jog
- Keyboard + LeftCtrl: Walk
- Keyboard + LeftShift: Run

### 推奨初期値

- WalkSpeed: 60
- JogSpeed: 160
- RunSpeed: 380
- WalkAnimSpeed: 0.75
- JogAnimSpeed: 1.0
- RunAnimSpeed: 1.25

## 13. Animation 選択仕様

UI は必ず animation 名で選択する。
番号入力は禁止。

内部保存は既存 runtime 互換を優先し、必要なら index を保持してよい。
ただし editor 表示、選択、再選択、repair は animation 名基準にする。

候補分類。

- Idle candidates
- Move candidates
- Attack candidates
- Dodge candidates
- All animations

## 14. StateMachine UI 仕様

必要ボタン。

- `Setup Full Player`
- `Setup Locomotion Only`
- `Add Light Attack`
- `Add Combo`
- `Repair Runtime Components`

可視化。

- Current State
- Previous State
- Last Transition
- Current Animation
- Current Gait
- MoveX / MoveY / MoveMagnitude / IsMoving
- Last Input
- Current Action

Graph の見た目改善は後でよい。
先に「押せば動く」を完成させる。

## 15. Action / Combo UI 仕様

Action は `ActionDatabaseComponent` を PlayerEditor でカード編集する。

Phase 1B は `Light1` 単発のみ。
`Light2 / Light3 / Heavy1 / RunLight` と combo link は Phase 2 で入れる。

UI は先に枠だけ作ってよいが、runtime 受け入れ条件は必ず `Light1` 単発を先にする。

表示名。

- `Light1`
- `Light2`
- `Light3`
- `Heavy1`
- `RunLight`

内部 index は現行互換のため使ってよい。
ただし UI に `nodeIndex` は出さない。

## 16. Timeline 連携

Timeline は animation ごとのイベント保存。
Action card から対象 Timeline を開けるようにする。

必要導線。

- Action card の `Timeline` ボタンで対象 animation の Timeline を表示。
- Hitbox / VFX / Audio / Shake / HitStop をその action timeline に追加。
- Combo input window を Timeline 上に表示する。
- Locomotion Phase 1A では Timeline は必須にしない。

## 17. 保存仕様

Save は prefab 一括保存。
個別 Save は不要。

保存対象。

- `InputActionMapComponent`
- `InputBindingComponent`
- `InputContextComponent`
- `InputUserComponent`
- `StateMachineAssetComponent`
- `StateMachineParamsComponent` の parameter 定義
- `ActionDatabaseComponent`
- `TimelineLibraryComponent`
- `ColliderComponent`
- `LocomotionStateComponent` の authored default
- `CharacterPhysicsComponent` の authored default

保存しないもの。

- current state
- playback current time
- resolved input current frame
- runtime hitbox
- audio runtime handle
- camera shake runtime state

scene load 後は必ず `PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents()` を通す。
ここを外すと prefab は正しいのに scene load 後だけ動かない事故が再発する。

## 18. Phase 別完成条件

### Phase 1A 完成条件

- `Setup Full Player` を押せる。
- prefab 保存できる。
- LevelEditor に配置できる。
- Play で Idle になる。
- WASD で Move になる。
- `MoveX / MoveY / MoveMagnitude / IsMoving` が見える。
- Current State / Current Animation が見える。
- scene save/load 後も同じように動く。

### Phase 1B 完成条件

- `LightAttack` で `Light1` が再生される。
- Attack state 表示になる。
- animation が切り替わる。
- 攻撃終了後に locomotion へ戻る。
- scene save/load 後も同じように動く。

### v1 完成条件

- Walk / Jog / Run が速度差で動く。
- Light combo がつながる。
- Heavy attack が動く。
- RunLight が動く。
- 攻撃 Timeline の Hitbox / Audio / Shake / HitStop が保存後も残る。
- UI から scancode / nodeIndex を直接触らない。

## 19. やってはいけない実装

- Phase 1A に攻撃や Timeline を混ぜて失敗原因を広げる。
- InputMap を外部 JSON に戻す。
- `StateMachineSystem` に locomotion 判定を重複実装する。
- parameter 定義を runtime system が勝手に増やす。
- Walk / Jog / Run 専用モーションを必須にする。
- PlayerEditor 専用の偽 runtime を作る。
- prefab と scene load で挙動を変える。

## 20. 最初にやる実装順

1. `Setup Full Player` を Phase 1A 内容だけで作る。
2. InputMap default を生成する。
3. Idle / Move StateMachine を生成する。
4. `InputResolveSystem` の axis 継続入力を直す。
5. locomotion parameter mirror を明確化する。
6. prefab 保存 -> LevelEditor 配置 -> Play の Idle / Move を確認する。
7. scene save/load 後も Idle / Move が残ることを確認する。
8. その後に `Light1` 単発攻撃へ進む。
