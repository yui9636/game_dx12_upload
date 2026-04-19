# PlayerEditor StateMachine 実行統合仕様書

本書は、`PlayerEditor` で設定した player prefab を `Hierarchy` に配置し、`GameLayer` の再生で `Idle / Move` がそのまま動くところまでを定義する。

前提として、保存・Prefab のひな型は既にある。  
ここで詰めるべき本丸は、**保存後の prefab が LevelEditor 配置を経て、GameLayer の runtime で自然に動くこと**である。

まずは `Idle / Move` の最小 locomotion に絞る。  
攻撃、回避、被弾、コンボ、Hitbox、VFX、Audio の完全連動は後回しにする。

## 1. 目的

`PlayerEditor` 上で作った state machine を、単なる編集データではなく runtime 動作に繋がるものにする。

最初の到達目標は次の 1 点である。

- `PlayerEditor` で設定した player prefab を保存し
- `Hierarchy` に配置し
- `Play` を押すと
- `GameLayer` 上で `WASD` や左スティック相当の入力により `Idle <-> Move` が切り替わる

この仕様の価値は、見た目のノード整理ではない。  
`入力 -> StateMachine 遷移 -> Animation 切替 -> Timeline 反映` を runtime 経路で通し、最後に `GameLayer` の中で本当に動かすことにある。

## 2. 対象範囲

今回の対象は次の範囲に限定する。

- `PlayerEditor` の state machine 設定
- prefab 保存
- `Hierarchy` に置いた prefab の起動
- `GameLayer` の runtime update 順
- `InputResolveSystem`
- `PlayerInputSystem`
- `LocomotionSystem`
- `StateMachineSystem`
- `PlaybackSystem`
- `AnimatorSystem`
- `PlayerRuntimeSetup`
- `PrefabSystem::InstantiatePrefab`

主な参照先:

- [Source/PlayerEditor/PlayerEditorPanel.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/PlayerEditor/PlayerEditorPanel.cpp)
- [Source/PlayerEditor/PlayerEditorSession.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/PlayerEditor/PlayerEditorSession.cpp)
- [Source/Gameplay/PlayerRuntimeSetup.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/PlayerRuntimeSetup.cpp)
- [Source/Asset/PrefabSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Asset/PrefabSystem.cpp)
- [Source/Hierarchy/HierarchyECSUI.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Hierarchy/HierarchyECSUI.cpp)
- [Source/Layer/GameLayer.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Layer/GameLayer.cpp)
- [Source/Gameplay/StateMachineSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/StateMachineSystem.cpp)
- [Source/Gameplay/LocomotionSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/LocomotionSystem.cpp)
- [Source/Input/InputResolveSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Input/InputResolveSystem.cpp)
- [Source/Animator/AnimatorSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Animator/AnimatorSystem.cpp)

## 3. 今回やらないこと

今回はやらない。やると散る。

- 攻撃、回避、被弾、コンボの完全実装
- Hitbox / VFX / Audio の完全同期
- Timeline の本格 authoring
- UE 風の graph 豪華化
- editor 専用の偽 state 切り替え
- preview-only の別 runtime 実装
- `std::min` / `std::max` を使う新規実装
- locomotion 以外を最初から全部つなぐこと

この段階で欲しいのは、見栄えではなく、保存したものがゲームで動くことだけである。

## 4. 現状の問題

現状の問題は「UI はあるが runtime に届く線が細い」ことだ。

具体的には次の通り。

- `PlayerEditor` で state machine は設定できるが、`GameLayer` で自然に動くところまでの仕様が弱い
- `pressed` ベースの条件は action 系には向くが、`Idle / Move` の継続判定には弱い
- `LocomotionSystem` は `moveInput` や `inputStrength` を計算しているのに、それを `StateMachineParamsComponent` へ渡す橋が明文化されていない
- `StateMachineSystem` は `Parameter` 条件を読めるのに、locomotion parameter の供給が弱い
- prefab 保存のひな型はあっても、配置後に runtime がちゃんと立ち上がることを仕様として固めないと崩れる
- editor 専用の state 切り替えを入れると、preview と GameLayer の差が広がる

要するに、今の危険は「設定 UI が足りない」よりも、「保存後の runtime 導線が曖昧」なことである。

## 5. 到達目標

最初の完成条件は、次の状態である。

- `PlayerEditor` で player prefab を保存できる
- `Hierarchy` にその prefab を置ける
- `Play` を押すと `GameLayer` でその player が動く
- `Idle` と `Move` が runtime 経路で切り替わる
- `Current State` / `Previous State` / `Last Transition` / `State Timer` が確認できる
- `MoveX` / `MoveY` / `MoveMagnitude` / `IsMoving` が runtime で見える
- `Animation` が state に応じて切り替わる
- editor 専用の偽 state 切り替えを使わない
- Timeline が未接続でも `Idle / Move` の完成を妨げない

この段階で UI の豪華さは完成条件にしない。  
勝敗は「ゲーム scene で動くか」で判定する。

## 6. Runtime 実行経路

`GameLayer::Update()` の順序は壊さない。  
runtime は既存の更新順をそのまま使う。

現行のゲーム側の流れは概ね次の通りである。

1. `InputContextSystem`
2. `InputResolveSystem`
3. `InputTextSystem`
4. `InputFeedbackSystem`
5. `PlayerInputSystem`
6. `ActionSystem`
7. `DodgeSystem`
8. `LocomotionSystem`
9. `StaminaSystem`
10. `HealthSystem`
11. `CharacterPhysicsSystem`
12. `PlaybackSystem`
13. `StateMachineSystem`
14. `TimelineSystem`
15. `TimelineHitboxSystem`
16. `TimelineVFXSystem`
17. `TimelineAudioSystem`
18. `TimelineShakeSystem`
19. `AnimatorSystem`

この仕様での重要点は次の 3 つである。

- input は `ResolvedInputStateComponent` に落とす
- locomotion の継続値は `LocomotionStateComponent` と `StateMachineParamsComponent` に同期する
- `StateMachineSystem` はその parameter を読んで遷移する

### 追加が必要な橋

`LocomotionSystem` が `inputStrength` を計算しても、それだけでは `StateMachineSystem` は動かない。  
`StateMachineSystem` が読む `MoveMagnitude` / `IsMoving` を `StateMachineParamsComponent` に mirror する bridge が必要である。

橋の置き方は 2 通りある。

- `LocomotionSystem` の直後に軽量 bridge system を置く
- 既存の locomotion 更新の中で `StateMachineParamsComponent` に mirror する

どちらでもよいが、**Preview と GameLayer で同じ経路を使うこと**を優先する。  
editor 専用の分岐は入れない。

## 7. Preview Run Mode

Preview Run Mode は、PlayerEditor 上で runtime の小型版を回すモードである。  
`見せるための再生` ではなく、`GameLayer と同じルールの事前確認` として扱う。

### 動作条件

- viewport にフォーカスがあるときだけ preview input を受ける
- `InputBindingComponent` が有効である
- `ResolvedInputStateComponent` が更新される
- `StateMachineParamsComponent` が runtime の値として扱われる
- `PlaybackComponent` と `AnimatorComponent` が実際に動く

### 振る舞い

- preview 開始時に runtime state を reset する
- preview 停止時に runtime state を元へ戻す
- editor 専用の state 切り替えボタンは置かない
- preview は runtime data を使うが、表示オーバーレイは editor 専用でもよい

### 禁止事項

- `Idle / Move` を editor 側の bool だけで偽装する
- preview 専用 state machine を別に作る
- `PlayerEditor` だけ更新順を変える
- preview 中に runtime と別の animation ルールを使う

## 8. Prefab / Hierarchy 連携

ここは保存済み prefab を `LevelEditor` のヒエラルキーに置くだけで起動できるようにする部分である。

### 前提

- `PlayerEditor` で prefab を保存できる
- `Hierarchy` で prefab を配置できる
- `PrefabSystem::InstantiatePrefab()` が player 系の最低限の authoring data を見つけたら `PlayerRuntimeSetup` を通す

### 現行コード上の意味

- `HierarchyECSUI.cpp` の `Gameplay > Player` は、すでに `PlayerRuntimeSetup::EnsurePlayerPersistentComponents()` / `EnsurePlayerRuntimeComponents()` / `ResetPlayerRuntimeState()` を呼んでいる
- `PrefabSystem.cpp` の `InstantiatePrefab()` も、最低限の player authoring components を見つけた場合に同じ `PlayerRuntimeSetup` を通している

つまり、仕様としては次の形に揃える。

- `PlayerEditor` で保存した prefab
- `Hierarchy` で手置きした player
- `Gameplay > Player` で生成した player

この 3 つは、runtime では同じ player ルートに収束しなければならない。

### 注意

`PrefabSystem` の minimum 判定はあくまで入口であり、見た目や設定の全責任を負わせてはいけない。  
`PlayerRuntimeSetup` は以下を分けて持つ。

- `EnsurePlayerPersistentComponents`
- `EnsurePlayerRuntimeComponents`
- `ResetPlayerRuntimeState`

さらに policy を分ける。

- persistent policy
- runtime policy
- reset policy

ここを分けないと、後で `PlayerRuntimeSetup.cpp` が何でも屋になる。

## 9. Preview Entity 構成

preview entity は dummy ではなく、runtime entity と同じ考え方で組む。

### 必須 component

- `TransformComponent`
- `MeshComponent`
- `PlayerTagComponent`
- `InputUserComponent`
- `InputBindingComponent`
- `InputContextComponent`
- `ResolvedInputStateComponent`
- `LocomotionStateComponent`
- `ActionStateComponent`
- `CharacterPhysicsComponent`
- `StateMachineParamsComponent`
- `PlaybackComponent`
- `AnimatorComponent`

### 将来拡張で必要になるもの

- `NodeSocketComponent`
- `TimelineComponent`
- `HitboxTrackingComponent`
- `HealthComponent`
- `StaminaComponent`

### 運用ルール

- 必須 component は `PlayerRuntimeSetup` で保証する
- preview reset 時は runtime state だけ戻す
- authoring data を壊さない
- preview entity は scene 保存や prefab 保存の主役にしない

## 10. Locomotion 最小 StateMachine 仕様

最初の StateMachine は `Idle` と `Move` だけでよい。  
これ以上は入れない。

### State

- `Idle`
- `Move`

### 遷移条件

- `Idle -> Move` : `IsMoving >= 1`
- `Move -> Idle` : `IsMoving <= 0`

### state の意味

- `Idle`
  - 立ち状態の base animation
  - `IsMoving == 0` の間はここに留まる
- `Move`
  - 移動中の base locomotion animation
  - `IsMoving == 1` の間はここに留まる

### state の責務

- state は input の生値を直接見ない
- state は parameter を読む
- state は animation 切替の入口になる
- `Move` state に将来 timelineAsset をぶら下げられる余地は残す

### 実装上の注意

- action / dodge / attack は入れない
- state machine を graph の豪華さで誤魔化さない
- animation は base locomotion を優先する

## 11. 入力と parameter

Locomotion では `pressed` を主役にしない。  
継続値を parameter として扱う。

### 使う parameter

- `MoveX`
- `MoveY`
- `MoveMagnitude`
- `IsMoving`

### 推奨定義

- `MoveX` / `MoveY` は `-1.0f` から `1.0f`
- `MoveMagnitude` は `0.0f` から `1.0f`
- `IsMoving` は binary parameter とする
- `MoveMagnitude` が deadzone を超えたら `IsMoving = 1`
- deadzone をまたぐだけで flicker しないように hysteresis を持たせる

### 入力ソース

- `WASD`
- 左スティック
- 必要なら gamepad axis

### 既存コードとの関係

- `InputResolveSystem` は `ResolvedInputStateComponent` を埋める
- `PlayerInputSystem` は `loco.moveInput = { input.axes[0], input.axes[1] }` を入れている
- `LocomotionSystem` は `inputStrength` と `gaitIndex` を計算している

この流れを使い、`StateMachineParamsComponent` に以下を mirror する。

- `MoveX`
- `MoveY`
- `MoveMagnitude`
- `IsMoving`

### 現実的な bridge

`StateMachineSystem` は `Parameter` 条件を読めるので、locomotion 用 parameter をそこへ渡すだけでよい。  
新しい巨大な state machine 言語は不要で、まずは bridge を通す。

## 12. UI 可視化要件

UI は飾りではなく debug tool である。  
少なくとも次を見えるようにする。

### StateMachine

- `Current State`
- `Previous State`
- `Last Transition`
- `State Timer`
- state ごとの animation 名
- state ごとの timeline asset の有無

### Input

- `MoveX`
- `MoveY`
- `MoveMagnitude`
- `IsMoving`
- 現在の binding 名
- deadzone の有無

### Animator

- 現在の animation
- current clip が再生中かどうか
- loop 状態
- preview で切り替わったかどうか

### Preview / Game

- Preview Run Mode の ON / OFF
- preview entity が runtime component を持っているか
- `Idle / Move` の現在値
- どの値で state が決まったか

### 禁止

- editor 専用 state 切り替えボタンを置く
- 状態表示をモーダルに隠す
- 数値だけ並べて意味が分からない UI にする

## 13. 実装フェーズ

### Phase 1

- prefab 保存済み player を `Hierarchy` に置く
- `PlayerRuntimeSetup` を通して runtime component を揃える
- `MoveX / MoveY / MoveMagnitude / IsMoving` を mirror する bridge を用意する
- `Idle / Move` の state machine を通す
- `GameLayer` で実際に動くことを確認する

### Phase 2

- `Current State` / `Previous State` / `Last Transition` / `State Timer` の debug 表示を入れる
- minimal state machine UI を整備する
- `Move` state の animation 選択を分かりやすくする
- preview run 中の input 状態を追いやすくする

### Phase 3

- `Move` state に timeline を optional 接続できるようにする
- footstep や簡易イベントの拡張余地を作る
- action / dodge / attack はこの後に分離して入れる

### Phase 4 以降

- 攻撃や回避などの state を追加する
- Hitbox / VFX / Audio 連動を足す
- graph や panel の見た目改善を行う

## 14. 完成条件

完成とみなす条件は、実際に手で確かめられること。

- `PlayerEditor` で player prefab を保存できる
- `Hierarchy` にその prefab を置ける
- `Play` を押すと `GameLayer` でその player が動く
- `WASD` または左スティック相当を入れると `IsMoving = 1` になる
- `Idle -> Move` が runtime 経路で発生する
- 入力を離すと `Move -> Idle` に戻る
- `Current State` / `Previous State` / `Last Transition` / `State Timer` が UI で確認できる
- 現在の animation が state に応じて切り替わる
- editor 専用の偽 state 切り替えを使っていない
- preview entity が runtime component を使っている
- Timeline が未接続でも Phase 1 は失敗扱いにならない
- 新規実装に `std::min` / `std::max` を使っていない

### 失敗例

- `PlayerEditor` だけ別の state machine を作る
- editor 専用の bool で state を偽装する
- `pressed` だけで `Idle / Move` を切り替える
- preview と runtime で component 構成を変える
- `Current State` を表示しないまま UI だけ増やす
- `Move` を作ったのに `AnimatorSystem` と `PlaybackComponent` に繋がっていない
- Timeline を必須にして最初の成功を遅らせる

この統合仕様の肝は、**保存した prefab が Hierarchy を経て GameLayer でそのまま動くこと**である。  
ここを外すと、また「見た目はあるが動かない」PlayerEditor に戻る。
