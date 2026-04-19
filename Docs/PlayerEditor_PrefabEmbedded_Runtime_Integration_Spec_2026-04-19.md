# PlayerEditor Prefab内包保存・Runtime統合仕様書

## 1. 目的

`PlayerEditor` の保存単位を **Prefab 1 個** に統一する。  
`StateMachine`、`Timeline`、`InputMap` を外部 JSON に保存せず、**Prefab に本体を内包**して保存する。

最終的な到達点は以下。

- `PlayerEditor` で model または prefab を開く
- `StateMachine / Timeline / Input` を編集する
- `Save` を押す
- prefab 1 個に全部入る
- `Hierarchy` にその prefab を置く
- `Play` を押す
- `GameLayer` で `Idle / Move` が runtime 経路でそのまま動く

この仕様は互換維持を目的にしない。  
**旧 path 参照構造は完全廃止前提**で進める。

## 2. 最重要方針

- `StateMachine` は外部 `.statemachine.json` に保存しない
- `Timeline` は外部 `.timeline.json` に保存しない
- `InputMap` は外部 `.inputmap.json` に保存しない
- `Prefab` が唯一の保存単位
- runtime は prefab 内包データだけを読む
- `assetPath / actionMapAssetPath / timelineAssetPath` は廃止する
- 旧 path ベース構造との互換は持たない
- editor 専用の偽 runtime は作らない
- `PlayerEditor -> Save -> Prefab -> Hierarchy -> Play -> GameLayer` の一直線を最優先にする

## 3. 現状の問題

現在の PlayerEditor 保存経路は、Prefab 本体保存ではなく **参照保存** である。

現状は実質こうなっている。

- `Timeline` を外部 `.timeline.json` に保存
- `StateMachine` を外部 `.statemachine.json` に保存
- `InputMap` を外部 `.inputmap.json` に保存
- prefab にはそれらの path だけを持たせる

この構造の問題は明確。

- prefab 1 個で閉じない
- 配置先で参照切れが起こりうる
- `Save` が「完成物保存」ではなく「参照ファイル保存の束」になっている
- `PlayerEditor` の編集対象が prefab なのか外部 asset なのか曖昧
- `Hierarchy` に置いたあとに `GameLayer` で同じ状態を再現しにくい
- state ごとの `Timeline Asset Path` は UI としても理解しにくい

辛口に言うと、今の構造は **Prefab authoring editor ではなく、参照配線 editor** でしかない。

## 4. 完全廃止するもの

今回の仕様で廃止対象とする path ベース要素は以下。

- `StateMachineParamsComponent.assetPath`
  - [StateMachineParamsComponent.h](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/StateMachineParamsComponent.h)
- `InputBindingComponent.actionMapAssetPath`
  - [InputBindingComponent.h](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Input/InputBindingComponent.h)
- `StateNode.timelineAssetPath`
  - [StateMachineAsset.h](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/PlayerEditor/StateMachineAsset.h)
- `TimelineAssetRuntimeBuilder::BuildFromPath()`
  - [TimelineAssetRuntimeBuilder.h](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/TimelineAssetRuntimeBuilder.h)
- `PlayerEditorSession` の個別 save/load document 経路
  - [PlayerEditorSession.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/PlayerEditor/PlayerEditorSession.cpp)

PlayerEditor では以下の概念も廃止する。

- `Timeline をファイルとして開く`
- `StateMachine をファイルとして開く`
- `InputMap をファイルとして開く`
- `Timeline / State / Input を個別保存する`

残すのは次だけ。

- `Open`
- `Save`

## 5. 新しい保存モデル

Prefab ルート entity に、authoring データ本体を component として持たせる。

### 5.1 新規 component

#### `StateMachineAssetComponent`

```cpp
struct StateMachineAssetComponent
{
    StateMachineAsset asset;
};
```

#### `TimelineLibraryComponent`

```cpp
struct TimelineLibraryComponent
{
    std::vector<TimelineAsset> assets;
    uint32_t nextTimelineId = 1;
};
```

#### `InputActionMapComponent`

```cpp
struct InputActionMapComponent
{
    InputActionMapAsset asset;
};
```

### 5.2 既存 component の役割変更

#### `StateMachineParamsComponent`

`assetPath` を削除する。  
この component は runtime 状態だけを持つ。

残すもの:

- `params`
- `paramCount`
- `currentStateId`
- `stateTimer`
- `animFinished`

#### `InputBindingComponent`

`actionMapAssetPath` を削除する。  
必要なら binding profile 系だけを保持する。

例:

- `bindingProfilePath`
- `runtimeOverrideProfilePath`

入力マップ本体は `InputActionMapComponent` が持つ。

#### `StateNode`

`timelineAssetPath` を削除する。  
代わりに prefab 内 timeline を指す ID を持つ。

```cpp
uint32_t timelineId = 0;
```

`0` は未設定とする。

## 6. Prefab 保存仕様

### 6.1 Save の意味

`PlayerEditor` の `Save` は **Prefab 保存のみ** とする。

保存時にやること:

1. editor working copy を preview root に書き戻す
2. `StateMachineAssetComponent` に state machine 本体を入れる
3. `TimelineLibraryComponent` に timeline 群を入れる
4. `InputActionMapComponent` に input map 本体を入れる
5. `NodeSocketComponent` を反映する
6. `PlayerRuntimeSetup` で player 必須 component を保証する
7. `PrefabSystem::SaveEntityToPrefabPath()` で prefab 保存する

### 6.2 Save 時にやってはいけないこと

- 外部 `.timeline.json` を保存しない
- 外部 `.statemachine.json` を保存しない
- 外部 `.inputmap.json` を保存しない
- path を component に書き込まない
- path fallback を温存しない

### 6.3 PrefabSystem の拡張

`PrefabSystem` は以下の component を serialize / deserialize 対象に追加する。

- `StateMachineAssetComponent`
- `TimelineLibraryComponent`
- `InputActionMapComponent`

逆に以下の field は削除対象。

- `StateMachineParamsComponent.assetPath`
- `InputBindingComponent.actionMapAssetPath`

対象:

- [PrefabSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Asset/PrefabSystem.cpp)

## 7. Open Model / Open Prefab 仕様

`Open Model` は実際には **model と prefab の両方** を開けるようにする。

### 7.1 対応拡張子

現行:

- `.fbx`
- `.gltf`
- `.glb`
- `.obj`

追加:

- `.prefab`

対象:

- [PlayerEditorPanel.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/PlayerEditor/PlayerEditorPanel.cpp)

### 7.2 `.prefab` を開いたときの動作

新規に `OpenPrefabFromPath()` を用意する。

手順:

1. `PrefabSystem::LoadPrefabSnapshot()` で snapshot を読む
2. PlayerEditor 所有の preview subtree として restore する
3. root entity の以下 component から working copy を復元する
   - `MeshComponent`
   - `StateMachineAssetComponent`
   - `TimelineLibraryComponent`
   - `InputActionMapComponent`
   - `NodeSocketComponent`
4. `m_currentModelPath` は preview root の `MeshComponent.modelFilePath` から取得する
5. `PlayerRuntimeSetup` を通して preview runtime component を保証する

### 7.3 model を開いたときの動作

model 単体を開いた場合は、今までどおり preview entity を作る。  
ただし authoring データは空で始める。

初期値:

- 空の `StateMachineAsset`
- 空の `TimelineLibraryComponent`
- 空の `InputActionMapAsset`
- 空の `NodeSocketComponent`

## 8. Bounds Fit Preview 仕様

モデルの大きさを slider で合わせるのは本質的に駄目。  
preview camera は **bounding box で自動フレーミング** する。

### 8.1 参考実装

既存の参考は次。

- [ThumbnailGenerator.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Asset/ThumbnailGenerator.cpp)
  - `ThumbnailGenerator::SetupCamera()`
- [EditorLayerSceneView.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Layer/EditorLayerSceneView.cpp)
  - `FocusEditorCameraOnTarget(...)` 周辺

### 8.2 framing ルール

#### 単体 model

- `Model::GetWorldBounds()` を使う
- `BoundingBox.Center` を注視点にする
- `BoundingBox.Extents` から radius を求める
- `distance = radius / sin(fov * 0.5f) * margin`
- `margin` は 1.2 から 1.35 の固定値

#### prefab / 複数 mesh

- preview subtree の world bounds を merge する
- `BoundingBox::CreateMerged()` で統合
- 統合 bounds を camera fit に使う

#### Humanoid の下寄せ

center をそのまま見ると頭寄りになりやすい。  
そのため target は次で補正する。

- `target = bounds.Center`
- `target.y += bounds.Extents.y * 0.10f`

### 8.3 実行タイミング

- `Open Model`
- `Open Prefab`
- `Reset Camera`
- `Focus Selected`

のタイミングで bounds fit を実行する。

### 8.4 やってはいけないこと

- モデル scale を変えて見かけ上合わせる
- viewport 内に余白前提の固定枠を作る
- 画面ごとに別々の fit 計算を持つ
- PlayerEditor だけ専用の雑な camera offset を増やす

## 9. GameLayer 接続仕様

ここは事故防止のため、更新順と責務を明文化する。

### 9.1 更新順

`GameLayer` では以下の順序を守る。

1. `InputResolveSystem`
2. `PlayerInputSystem`
3. `Locomotion bridge`
4. `LocomotionSystem`
5. `PlaybackSystem`
6. `StateMachineSystem`
7. `TimelineSystem`
8. `TimelineHitboxSystem`
9. `TimelineVFXSystem`
10. `TimelineAudioSystem`
11. `TimelineShakeSystem`
12. `AnimatorSystem`

### 9.2 順序の意味

- `InputResolveSystem`
  - 内包 `InputActionMap` を使って `ResolvedInputStateComponent` を更新する
- `PlayerInputSystem`
  - input から locomotion 用の raw 入力を作る
- `Locomotion bridge`
  - `MoveX / MoveY / MoveMagnitude / IsMoving` を `StateMachineParamsComponent` に mirror する
- `LocomotionSystem`
  - 移動系の派生値を更新する
- `PlaybackSystem`
  - 現在 animation の時間進行を更新する
- `StateMachineSystem`
  - 内包 `StateMachineAsset` を読んで state 遷移する
- `TimelineSystem` 群
  - 遷移済み state の timeline を処理する
- `AnimatorSystem`
  - 最終的な base animation を反映する

### 9.3 禁止事項

- `StateMachineSystem` の中で input を直接読む
- `PlayerEditor` だけ別の更新順を使う
- preview でだけ shortcut 的に state を切り替える
- `LocomotionSystem` の前に state machine を動かす

## 10. Component 配置責務

ここを曖昧にすると、実装時に root と preview の責務が崩れる。

### 10.1 Prefab root が持つもの

Prefab root に置く component:

- `MeshComponent`
- `NodeSocketComponent`
- `StateMachineAssetComponent`
- `TimelineLibraryComponent`
- `InputActionMapComponent`
- `PlayerTagComponent`
- `CharacterPhysicsComponent`
- `InputUserComponent`
- `InputContextComponent`
- `InputBindingComponent`

Prefab root に置かない runtime 専用 state:

- `ResolvedInputStateComponent`
- `PlaybackComponent`
- `TimelineComponent`
- `TimelineItemBuffer`
- `AnimatorComponent`

### 10.2 Preview root の責務

PlayerEditor preview root は次を兼ねる。

- authoring データの仮置き場所
- runtime preview 用 entity

ただし preview root は scene 永続物ではない。  
editor セッション中だけ生きる。

### 10.3 Instantiate 後の責務

`PrefabSystem::InstantiatePrefab()` 後に `PlayerRuntimeSetup` を通し、

- persistent component の不足を補う
- runtime component を生成する
- runtime state を reset する

authoring data を持つのは prefab root。  
runtime state を持つのは instantiate 後の live entity。

### 10.4 禁止事項

- `StateMachineAssetComponent` を child 側へ分散配置しない
- timeline library を state ごとにコピーしない
- preview 用だけ別の authoring storage を持たない
- runtime state を prefab 保存しない

## 11. Runtime 読取仕様

runtime は path を読まない。  
entity に載っている内包 component を直接読む。

### 11.1 `StateMachineSystem`

変更後:

- 同じ entity の `StateMachineAssetComponent` を読む
- state 遷移時は `state.timelineId` を参照する
- `TimelineLibraryComponent.assets` から一致 timeline を取る
- `TimelineAssetRuntimeBuilder::Build(...)` を直接呼ぶ

対象:

- [StateMachineSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/StateMachineSystem.cpp)

### 11.2 `InputResolveSystem`

変更後:

- 同じ entity の `InputActionMapComponent.asset` を読む
- `actionMapAssetPath` は使わない
- cache loader は Player runtime では不要

対象:

- [InputResolveSystem.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Input/InputResolveSystem.cpp)

### 11.3 `TimelineAssetRuntimeBuilder`

`BuildFromPath()` は廃止。  
残す API は `Build(const TimelineAsset&, ...)` のみ。

対象:

- [TimelineAssetRuntimeBuilder.h](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/TimelineAssetRuntimeBuilder.h)
- [TimelineAssetRuntimeBuilder.cpp](/C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Source/Gameplay/TimelineAssetRuntimeBuilder.cpp)

## 12. Editor UI 変更仕様

### 12.1 上部ツールバー

維持するもの:

- `Open`
- `Save`

追加候補:

- `Reset Camera`

削除済み方針を維持するもの:

- `Timeline` 個別 open/save
- `State` 個別 open/save
- `Input` 個別 open/save

### 12.2 State の timeline 設定

今の `Timeline Asset Path` 欄は廃止。

置き換え:

- `Timeline`
  - `None`
  - `Timeline 01`
  - `MoveLoop`
  - `Footstep`
 などの **内包 timeline 一覧コンボ**

StateNode は `timelineId` だけ持つ。

### 12.3 dirty 管理

document 単位 dirty は捨てる。  
Prefab authoring 全体で 1 dirty に統一する。

対象:

- state machine 編集
- timeline 編集
- input 編集
- socket 編集
- model binding 変更

## 13. 実装フェーズ

### Phase 1: データ構造の置換

- `StateMachineAssetComponent` 追加
- `TimelineLibraryComponent` 追加
- `InputActionMapComponent` 追加
- `StateNode.timelineAssetPath` を `timelineId` に置換
- `StateMachineParamsComponent.assetPath` 削除
- `InputBindingComponent.actionMapAssetPath` 削除

### Phase 2: Prefab 保存一本化

- `PrefabSystem` に新 component の serialize/deserialize を追加
- `PlayerEditorSession::SavePrefabDocument()` を内包保存専用に変更
- `SaveAllDocuments()` と個別 serializer 経路を削除

### Phase 3: runtime 直結化

- `StateMachineSystem` を direct component 読みへ変更
- `InputResolveSystem` を direct component 読みへ変更
- `TimelineAssetRuntimeBuilder::BuildFromPath()` を削除

### Phase 4: Open Prefab / Bounds Fit

- `Open` に `.prefab` を追加
- `OpenPrefabFromPath()` 実装
- preview subtree bounds merge 実装
- `ThumbnailGenerator::SetupCamera()` を参考に bounds fit 実装

### Phase 5: UI cleanup

- `Timeline Asset Path` UI 削除
- timeline 選択コンボへ置換
- preview scale を debug 扱いに格下げ
- `Open Model` を `Open` へ改名

## 14. 完成条件

以下を満たしたら完了とする。

- `PlayerEditor` の `Save` で prefab 1 個だけ保存される
- prefab の中に `StateMachine / Timeline / InputMap` 本体が入っている
- `StateMachineParamsComponent.assetPath` が存在しない
- `InputBindingComponent.actionMapAssetPath` が存在しない
- `StateNode.timelineAssetPath` が存在しない
- `TimelineAssetRuntimeBuilder::BuildFromPath()` が存在しない
- `Open` で `.prefab` を選択できる
- `.prefab` を開いたとき、中の model / state machine / timeline / input / sockets が PlayerEditor に復元される
- model または prefab を開いたとき、bounds fit で camera 内に収まる
- preview scale を触らなくても通常サイズの model が見切れない
- `Hierarchy` に prefab を置いて `Play` を押すと `GameLayer` で `Idle / Move` が動く
- `GameLayer` の更新順が仕様書どおり固定されている
- Prefab root と live runtime entity の責務分離が守られている

## 15. やってはいけない実装

- path fallback を残す
- 旧 JSON asset save を温存する
- prefab と外部 asset の二重管理を続ける
- runtime が serializer を叩いてファイルを読む
- `Timeline Asset Path` を string input のまま残す
- model scale で camera framing を誤魔化す
- PlayerEditor だけ別 runtime を作る
- 互換を理由に中途半端な二重構造を残す
- GameLayer 更新順を実装者判断で入れ替える
- authoring component と runtime component を同じ責務で混ぜる

## 16. 結論

今回の改修は小修正ではない。  
**PlayerEditor を「外部 asset 参照 editor」から「Prefab authoring editor」に作り替える** 改修である。

正しい完成形は次の 5 点。

- `Prefab 1 個に全部入る`
- `Open` で `.prefab` も開ける
- `bounds fit` で最初から見える
- `Hierarchy` に置いて `Play` すればそのまま動く
- `GameLayer` の更新順と component 責務が文書どおり固定される

ここを外すと、また UI だけ増えて中身が散る。
