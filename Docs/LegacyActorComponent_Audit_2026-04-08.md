# Legacy Actor / Component Audit

対象:
- `Actor` / `ActorManager`
- `Component` 継承ベースの旧コンポーネント
- ECS 化後も残っている旧ランタイム入口

## 結論

現時点で旧基盤は 3 グループに分かれる。

1. 即削除候補
2. まだ現役で、先に ECS 置換が必要なもの
3. 基盤は残っているが、参照元を消せばまとめて落とせるもの

## 1. 即削除候補

### `Source/Stage/StageInfoComponent.h`
### `Source/Stage/StageInfoComponent.cpp`

- リポジトリ内で自己定義以外の参照が見つからない
- `Component` 継承の旧コンポーネント
- ECS 側への接続もなし

判定:
- 削除候補

### `Source/Storage/GEStorageCompilerComponent.h`
### `Source/Storage/GEStorageCompilerComponent.cpp`

- リポジトリ内で自己定義以外の参照が見つからない
- `Component` 継承の旧コンポーネント
- 実際には utility / storage 機能であり、Actor component として持つ意味が薄い

判定:
- 旧 component としては削除候補
- 必要なら static utility か ECS/editor service へ再配置

### `Source/Collision/ColliderComponent.h`
### `Source/Collision/ColliderComponent.cpp`

- 旧 `class ColliderComponent : public Component`
- すでに ECS 版の `Source/Component/ColliderComponent.h` が主系
- 旧版の参照は実質的に自己定義とコメントだけ
- ECS 系の `CollisionSystem.cpp` は `Source/Component/ColliderComponent.h` を使っている

判定:
- 旧版は削除候補

### `ShadowMap::DrawSceneImmediate`

対象:
- `Source/ShadowMap.h`
- `Source/ShadowMap.cpp`

- `std::vector<std::shared_ptr<Actor>>` を受ける旧描画経路
- 宣言と定義は残っているが、呼び出し元が見当たらない

判定:
- 参照ゼロなら削除候補

## 2. まだ現役で、先に置換が必要なもの

### `Source/Animator/AnimatorComponent.h`
### `Source/Animator/AnimatorComponent.cpp`

- `class AnimatorComponent : public Component`
- まだ以下から参照されている
  - `Source/Cinematic/CinematicSequencerComponent.cpp`
  - `Source/Cinematic/SequencerDriver.h`
  - `Source/PlayerEditor/PreviewState.cpp`
  - `Source/PlayerEditor/TimelineDriver.cpp`

判定:
- まだ削除不可
- ECS アニメーション authoring / preview / cinematic bridge が先

### `Source/Component/NodeAttachComponent.h`
### `Source/Component/NodeAttachComponent.cpp`

- 名前空間上は `Component` 配下だが、中身は旧 Actor component
- `class NodeAttachComponent : public Component`
- まだ以下から参照されている
  - `Source/Collision/CollisionSystem.cpp`
  - `Source/DebugRender/DebugRenderSystem.cpp`
  - `Source/PlayerEditor/PlayerEditorPanel.h`
  - 旧 `Source/Collision/ColliderComponent.cpp`

判定:
- まだ削除不可
- `bone/socket attachment` を ECS utility か ECS component/system へ移す必要がある

### `Source/Cinematic/CinematicSequencerComponent.h`
### `Source/Cinematic/CinematicSequencerComponent.cpp`

- `class CinematicSequencerComponent : public Component`
- `ActorManager` と `Actor` を直接使う
- editor ghost actor、selected actor、`AnimatorComponent` 取得が残っている

判定:
- 旧 Actor 基盤の主要依存先
- Cinematic editor を ECS entity ベースへ切り替えるまで削除不可

### `Source/Cinematic/CinematicPlayComponent.h`
### `Source/Cinematic/CinematicPlayComponent.cpp`

- `class CinematicPlayComponent : public Component`
- `ActorManager::Instance().GetActors()` を参照
- 現状、明示的な利用先は少ないが旧コンポーネントであることは確実

判定:
- 利用頻度は低そうだが、旧 component として残っている
- Cinematic 系整理時にまとめて削除候補

## 3. 参照元を消せばまとめて落とせる基盤

### `Source/Actor/Actor.h`
### `Source/Actor/Actor.cpp`

- 直接の `: public Actor` 派生はほぼ消えている
- ただし以下がまだ依存
  - `CinematicPlayComponent`
  - `CinematicSequencerComponent`
  - `Component` 基底クラス
  - `UndoSystem` 内の旧 Actor undo commands
  - `Camera/CameraController.cpp`
  - `Cinematic/CinematicTrack.h`
  - `ShadowMap::DrawSceneImmediate`

判定:
- 現時点では削除不可
- ただし依存先はかなり限られている

### `ActorManager`

現役参照:
- `Source/Cinematic/CinematicPlayComponent.cpp`
- `Source/Cinematic/CinematicSequencerComponent.cpp`
- `Source/System/UndoSystem.h`

コメント参照のみ:
- `Source/Stage/Stage.cpp`

判定:
- Cinematic と旧 undo commands を落とせば削除圏内

### `Source/Component/Component.h`

- 旧 Actor component 基底
- これを継承している現存クラス:
  - `AnimatorComponent`
  - `CinematicPlayComponent`
  - `CinematicSequencerComponent`
  - 旧 `Collision::ColliderComponent`
  - `NodeAttachComponent`
  - `StageInfoComponent`
  - `GEStorageCompilerComponent`

判定:
- 上記を全移行・削除した時点で削除可能

## Undo 系の状態

### `Source/System/UndoSystem.h`

このファイルはすでに ECS undo が主流になっているが、旧 Actor 用 command もまだ同居している。

旧 Actor command:
- `CmdTransform`
- `CmdCreate`
- `CmdDelete`
- `CmdParent`

特徴:
- `Actor`
- `ActorManager`
- `shared_ptr<Actor>`
に依存している

一方で、実際の editor 側の利用は ECS undo API が主流:
- `ExecuteAction`
- `RecordAction`
- `Undo(Registry&)`
- `Redo(Registry&)`

判定:
- 旧 command 群は分離・削除候補
- `UndoSystem` 本体は残す

## 削除優先順位

### Phase A: すぐ削除してよい候補

1. `Source/Stage/StageInfoComponent.*`
2. `Source/Storage/GEStorageCompilerComponent.*`
3. 旧 `Source/Collision/ColliderComponent.*`
4. `ShadowMap::DrawSceneImmediate` の旧 Actor 経路

### Phase B: 先に置換してから消す候補

1. `CinematicPlayComponent`
2. `CinematicSequencerComponent`
3. `ActorManager`
4. `Actor`
5. `Component` 基底
6. `UndoSystem` 内の旧 Actor commands

### Phase C: 最後に残る大物

1. `AnimatorComponent`
2. `NodeAttachComponent`

この 2 つは editor / animation / socket attachment の代替を用意してから消すべき。

## 実装上の次アクション

1. Phase A を削除
2. `UndoSystem.h` から旧 Actor commands を分離・削除
3. `CinematicPlayComponent / CinematicSequencerComponent` を ECS entity ベースへ置換
4. 参照が消えた段階で `Actor / ActorManager / Component` を削除

