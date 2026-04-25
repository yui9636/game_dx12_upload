# PlayerEditor StateMachine Ownership 仕様書

## 1. 目的

PlayerEditor で作った Player prefab は、GameLayer 再生時に **StateMachine を最上位の行動制御** として動作すること。

入力、移動、攻撃、回避、アニメーション切替、Timeline 発火は、最終的に StateMachine の現在 State を通って実行される。

`PlayerInputSystem` や `ActionSystem` が StateMachine を迂回して攻撃や行動遷移を開始する実装は禁止する。

## 2. 現在の問題

現状は「StateMachine で全管理」というゴールに対して、実装が二重経路になっている。

- `PlayerInputSystem.cpp` が `LightAttack` 入力を検知し、`ActionStateComponent.reservedNodeIndex` を直接立てている。
- `ActionSystem.cpp` が `reservedNodeIndex` を見て、StateMachine に `Light1` state が無くても攻撃を開始している。
- そのため、StateMachine UI 上では `Idle / Move` しか無いのに攻撃が再生される。
- `ActionSystem` が独自に `ActionStateComponent.state` と `PlaybackComponent` を操作するため、StateMachine の `currentStateId` と実際の行動がズレる。
- `GameLayer.cpp` の更新順が `ActionSystem -> LocomotionSystem -> PlaybackSystem -> StateMachineSystem` になっており、StateMachine が最後に評価されるため、行動の入口として弱い。

これは仕様として不正。

攻撃が StateMachine グラフに無いなら、攻撃は起動してはいけない。

## 3. 最重要方針

StateMachine を source of truth にする。

- `StateMachineAssetComponent` が authored graph。
- `StateMachineParamsComponent.currentStateId` が現在行動の正。
- `ActionStateComponent` は既存 system 用の mirror。正ではない。
- `ActionDatabaseComponent` は攻撃 payload のデータ置き場。遷移開始権限は持たない。
- `PlayerInputSystem` は入力値と trigger parameter を作るだけ。行動を開始しない。
- `ActionSystem` はコンボ window や action payload 補助だけ。State 遷移を開始しない。
- `AnimatorService::PlayBase` / `PlayAction` の呼び出しは StateMachine の state enter を主経路にする。

## 4. 禁止事項

以下は禁止。

- `PlayerInputSystem` が `reservedNodeIndex` を直接設定して攻撃を開始する。
- `ActionSystem` が `Locomotion -> Action` 遷移を独自判断する。
- StateMachine に存在しない攻撃を `ActionDatabaseComponent` だけで再生する。
- Editor 専用の偽 state 切替で Preview だけ動かす。
- `ActionStateComponent.state` を正の現在 State として扱う。
- `StateMachineSystem` の外で `StateMachineParamsComponent.currentStateId` を勝手に変更する。

## 5. Component の責務

### StateMachineAssetComponent

Player prefab に保存される authored graph。

`Idle / Move / Light1 / Dodge / Damage` など、プレイヤーの行動 node はここに必ず存在する。

### StateMachineParamsComponent

Runtime の StateMachine 評価用。

- `currentStateId`
- `stateTimer`
- `animFinished`
- `MoveX`
- `MoveY`
- `MoveMagnitude`
- `IsMoving`
- `LightAttack`
- `HeavyAttack`
- `Dodge`

trigger 系 parameter は入力 frame で立ち、StateMachine 評価後に消費される。

### ActionDatabaseComponent

攻撃 state の payload 置き場。

v1 では `StateNode::properties["ActionNodeIndex"]` で ActionNode を参照する。

ただし、ActionDatabase は state 遷移を開始しない。

### ActionStateComponent

既存の `LocomotionSystem` / `CharacterPhysicsSystem` / `DodgeSystem` などが参照する coarse state mirror。

StateMachineSystem が現在 StateNodeType から同期する。

- `StateNodeType::Locomotion` -> `CharacterState::Locomotion`
- `StateNodeType::Action` -> `CharacterState::Action`
- `StateNodeType::Dodge` -> `CharacterState::Dodge`
- `StateNodeType::Damage` -> `CharacterState::Damage`
- `StateNodeType::Dead` -> `CharacterState::Dead`

## 5.1 ActionSystem の非責務

`ActionSystem` は今後、Player 行動の司令塔にしない。

ActionSystem がやってよいこと:

- 現在 Action state の補助 runtime 値を更新する。
- combo window の入力受付可能時間を計算する。
- damage / cancel / magnetism など ActionDatabase payload 由来の補助値を処理する。
- StateMachine が選んだ Action state に対して、payload を読む。

ActionSystem がやってはいけないこと:

- 入力から攻撃開始を判断する。
- `ActionStateComponent.reservedNodeIndex` を見て state を開始する。
- `StateMachineParamsComponent.currentStateId` を変更する。
- `PlaybackComponent` を使って独自に action 終了遷移を決める。
- `AnimatorService::PlayAction` を state enter 以外の入口から呼ぶ。

v1 では、ActionSystem は空に近くてよい。

攻撃が StateMachine 経由で再生され、Timeline が発火するだけなら、ActionSystem はまだ重くしない。

## 6. Runtime 更新順

GameLayer の Player runtime 更新順は以下を目標にする。

1. `InputContextSystem`
2. `InputResolveSystem`
3. `PlayerInputSystem`
4. `PlaybackSystem`
5. `StateMachineSystem`
6. `LocomotionSystem`
7. `CharacterPhysicsSystem`
8. `TimelineSystem`
9. `ActionSystem`

理由:

- 入力を先に解決する。
- `PlayerInputSystem` は parameter と `LocomotionStateComponent.moveInput` を更新する。
- `PlaybackSystem` は現在 state の再生時間を進め、`finished` を確定する。
- `StateMachineSystem` は入力・parameter・AnimEnd を評価し、state enter を実行する。
- `LocomotionSystem` は StateMachine が同期した `ActionStateComponent` を見て、Locomotion state の時だけ移動を適用する。
- `ActionSystem` は最後に補助処理だけを行う。行動開始はしない。

## 7. PlayerInputSystem 仕様

`PlayerInputSystem` は行動を開始しない。

やること:

- `MoveX / MoveY` を `LocomotionStateComponent.moveInput` に反映。
- `MoveMagnitude / IsMoving` の元になる入力値を更新。
- `LightAttack / HeavyAttack / Dodge` の trigger parameter を `StateMachineParamsComponent` に反映。

trigger の寿命:

- `LightAttack / HeavyAttack / Dodge` は 1 frame trigger とする。
- `PlayerInputSystem` が pressed edge を検出した frame だけ `1.0f` にする。
- `StateMachineSystem` は transition 評価後、消費対象 trigger を必ず `0.0f` に戻す。
- transition が成立しなかった trigger も、その frame の StateMachine 評価後に `0.0f` に戻す。
- trigger を `pressed` の継続状態として扱わない。

理由:

- 攻撃ボタン押しっぱなしで毎フレーム再突入する事故を防ぐ。
- `Idle -> Light1` と `Move -> Light1` のどちらにも同じ trigger を安全に使う。
- Preview と GameLayer の入力挙動を揃える。

やらないこと:

- `ActionStateComponent.reservedNodeIndex` を設定しない。
- `ActionStateComponent.state` を変更しない。
- `AnimatorService` を呼ばない。
- `PlaybackComponent` を直接リセットしない。

## 8. StateMachineSystem 仕様

StateMachineSystem は Player 行動の唯一の入口。

State enter 時に行うこと:

- `StateMachineParamsComponent.currentStateId` を更新。
- `StateMachineParamsComponent.stateTimer` を 0 に戻す。
- `PlaybackComponent` を state animation 用に初期化。
- `AnimatorService` で state animation を再生。
- `TimelineLibraryComponent` から state に紐づく Timeline を runtime へ展開。
- `ActionStateComponent` を StateNodeType から同期。
- Action state へ入る場合は水平 velocity を 0 にする。

State enter の二重実行防止:

- State enter は `currentStateId` が変わった瞬間だけ実行する。
- 同じ state に留まっている frame では、`PlaybackComponent` を 0 に戻さない。
- 同じ state に留まっている frame では、`AnimatorService::PlayBase` / `PlayAction` を再呼び出ししない。
- 初期化時だけ `currentStateId == 0` を default state enter として扱う。
- transition 評価で `toState == currentStateId` の場合は、明示的な self transition として扱う時だけ enter を許す。v1 では self transition は未対応でよい。

State exit / transition 時に行うこと:

- consumed trigger を下げる。
- 前 state の transient timeline を破棄。
- 次 state の timeline を有効化。

AnimEnd の生成元:

- `PlaybackSystem` が `PlaybackComponent.finished` を立てる。
- `StateMachineSystem` がその値を読み、`StateMachineParamsComponent.animFinished` に mirror する。
- `ConditionType::AnimEnd` は `StateMachineParamsComponent.animFinished` だけを見る。
- `ActionSystem` は AnimEnd を生成しない。
- `AnimatorComponent` の内部状態を直接 AnimEnd 条件に使わない。
- loop state では原則 `PlaybackComponent.finished` は立たないため、`AnimEnd` 遷移には使わない。

## 9. LightAttack 最小仕様

Phase 1B の攻撃は `Light1` 単発だけ。

必要な State:

- `Idle`: `StateNodeType::Locomotion`, loop animation
- `Move`: `StateNodeType::Locomotion`, loop animation
- `Light1`: `StateNodeType::Action`, non-loop animation

必要な Transition:

- `Idle -> Move`: `IsMoving >= 1`
- `Move -> Idle`: `IsMoving <= 0`
- `Idle -> Light1`: `LightAttack == 1`
- `Move -> Light1`: `LightAttack == 1`
- `Light1 -> Idle`: `AnimEnd == 1`

重要:

- `Light1` state が無い prefab では、`LightAttack` を押しても攻撃してはいけない。
- `ActionDatabaseComponent` に node 0 があっても、StateMachine に `Light1` state が無いなら攻撃してはいけない。
- 移動中に攻撃しても、攻撃終了後はいったん `Idle` に戻す。
- 移動入力を押し続けている場合は、次の StateMachine 評価で `Idle -> Move` してよい。

## 10. Animation と ActionDatabase の関係

v1 では animation の source of truth は `StateNode.animationIndex`。

`ActionDatabaseComponent` は以下の補助情報を持つ。

- damage
- input window
- combo window
- cancel window
- next action
- magnetism

`ActionDatabaseComponent.nodes[n].animIndex` は互換用に残してよいが、StateMachine 経由では `StateNode.animationIndex` を優先する。

## 11. Timeline の扱い

Timeline は Player prefab 内に 1 つだけ存在する animation event document。

StateMachine で Timeline asset を選択する UI は廃止する。

理由:

- 現在の PlayerEditor では Timeline は外部 asset を複数差し替える設計ではない。
- prefab 内蔵 Timeline を 1 つの正とする方が保存・配置・GameLayer 再生の経路が単純。
- State ごとに Timeline を選ばせる UI は、実際には不要な選択肢を増やして混乱させる。
- 「None / Current Timeline #1」のような UI は消す。

自動反映ルール:

- StateMachine は Timeline を選択しない。
- State は `animationIndex` だけを持つ。
- Runtime は現在 State の `animationIndex` を使って、prefab 内蔵 Timeline から同じ animation 用 event だけを抽出する。
- Hitbox / Audio / CameraShake / HitStop は、現在 animation に紐づく Timeline event だけが有効。
- 別 animation の Timeline event が混ざって発火してはいけない。
- `StateNode.timelineId` は v1 UI では表示しない。
- 既存データに `timelineId` が残っていても、v1 runtime では prefab 内蔵 Timeline の animation binding を優先する。

StateMachine UI から削除するもの:

- Timeline combo box
- Timeline path
- Timeline asset picker
- None / Current Timeline selection
- State property 内の Timeline 手動指定欄

Timeline UI に残すもの:

- Timeline パネル本体
- animation ごとの event track
- Hitbox / Audio / CameraShake / HitStop event 編集
- 現在選択 animation への自動 binding 表示

StateMachine に置くべき情報:

- State name
- State type
- Animation
- Loop
- Speed
- Can Interrupt
- Transition
- Condition

StateMachine に置かない情報:

- Timeline asset path
- Timeline 選択
- Timeline document 選択
- event track 編集 UI

## 12. Editor UI 要件

PlayerEditor の StateMachine UI は、実行経路を隠してはいけない。

必須表示:

- Current State
- Previous State
- Last Transition
- Current Animation
- MoveX
- MoveY
- MoveMagnitude
- IsMoving
- LightAttack trigger
- ActionState mirror

`Setup Full Player` は最低限以下を作る。

- `Idle`
- `Move`
- `Light1`
- `Idle -> Move`
- `Move -> Idle`
- `Idle -> Light1`
- `Move -> Light1`
- `Light1 -> Idle`
- `LightAttack` input binding

## 13. 実装フェーズ

### Phase A: 直通攻撃の停止

- `PlayerInputSystem` から `reservedNodeIndex` 設定を削除。
- StateMachine に無い攻撃は起動しないようにする。
- `ActionSystem` の `Locomotion -> Action` 独自遷移を停止。

完成条件:

- StateMachine が `Idle / Move` だけの prefab で `LightAttack` を押しても攻撃しない。
- StateMachine に `Light1` を追加すると攻撃する。

### Phase B: StateMachine enter 処理の正規化

- `StateMachineSystem` が state enter 時に `PlaybackComponent` / `AnimatorService` / `Timeline` / `ActionStateComponent` を同期する。
- Action state 中は `LocomotionSystem` が移動を適用しない。
- 攻撃終了後は `Light1 -> Idle` の `AnimEnd` 遷移で戻る。

完成条件:

- Idle 中攻撃 -> Light1 -> Idle。
- Move 中攻撃 -> Light1 -> Idle。
- 移動押しっぱなしなら、その後 Idle -> Move。

### Phase C: ActionDatabase の payload 化

- `ActionDatabaseComponent` を遷移実行者ではなく payload 参照にする。
- `StateNode::properties["ActionNodeIndex"]` で node を参照。
- combo は StateMachine transition と ActionDatabase payload の両方を使って表現する。

完成条件:

- ActionDatabase node だけでは攻撃しない。
- StateMachine node がある時だけ攻撃する。

### Phase D: 更新順の修正

- GameLayer の Player runtime order を仕様順へ変更する。
- Preview Run Mode も同じ順序へ寄せる。

完成条件:

- Preview と GameLayer で Current State の遷移順が一致する。
- AnimEnd 遷移が 1 回で安定して発火する。

## 14. 受け入れ条件

- `Idle / Move` だけの StateMachine では、攻撃入力を押しても攻撃 animation が再生されない。
- `Setup Full Player` 後は StateMachine に `Light1` が見える。
- `Idle -> Light1 -> Idle` が成立する。
- `Move -> Light1 -> Idle` が成立する。
- 攻撃終了後にフリーズしない。
- 攻撃終了後に `ActionStateComponent` が `Locomotion` mirror に戻る。
- `ActionDatabaseComponent` の node だけで攻撃が起動しない。
- Save -> LevelEditor 配置 -> Play -> 入力確認で、Preview と GameLayer の挙動が一致する。

## 15. 結論

現在の問題は「攻撃が動くかどうか」ではなく、「攻撃が StateMachine を通らずに動いてしまうこと」。

ゴールは、PlayerEditor で見えている StateMachine graph と GameLayer runtime の行動が一致すること。

そのため、直通経路は便利でも廃止する。
