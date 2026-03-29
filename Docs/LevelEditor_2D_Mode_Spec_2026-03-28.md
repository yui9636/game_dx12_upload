# レベルエディター 2D Mode 仕様書

更新日: 2026-03-28

## 目的

- レベルエディターに **2D 編集モード** を追加する
- 将来の **タイトル画面 / UI 演出 / 2D 演出シーン** をエディター上で直接構築できるようにする
- 既存の ECS / Undo / Prefab / Scene 保存導線を再利用し、3D モードとは別系統にせず統合運用する

## 基本方針

- 2D Mode は「別エディター」ではなく **Level Editor の view mode 切替** として実装する
- Scene 自体は ECS 共通で保持し、**2D / 3D は編集・表示方法だけを切り替える**
- タイトル画面用途を優先し、初期版は **UI レイアウト寄りの 2D 配置** に強い設計とする
- 既存 3D ワークフローを壊さないことを最優先とする
- 実装は既存の `Source/UI` 系コードを無視せず、**それを editor 連携可能な形へ発展** させる

## 旧UI基盤の扱い（更新）

- Source/UI 配下の旧 UIManager / UIElement / UIScreen / UIWorld は、**互換維持対象ではなく廃止対象** とする。
- 旧UI基盤は現段階で実運用されておらず、現行の DX12 / command list / ECS editor workflow と整合しないため、設計上の中心には置かない。
- 2D/UI authoring と runtime 描画の正は **ECS component** とし、以下を正式採用する。
  - RectTransformComponent
  - CanvasItemComponent
  - SpriteComponent
  - TextComponent
  - Camera2DComponent
- runtime 側の 2D/UI 描画も editor と同じ ECS scene を直接読む方式へ統一する。
- Source/UI 側への adapter / migration は必須要件から外し、必要なら一時的な移行補助に限定する。
- 最終的なゴールは、**旧UIクラスを参照しない pure ECS 2D/UI runtime** の成立である。

## 2D Mode の再定義

- これは「Sprite を置ける簡易モード」ではなく
- **ECS ベースの 2D / UI scene authoring 基盤**
を作るフェーズである

そのため、最低限以下が必要:

- ECS 仕様の 2D UI component 群
- font 管理
- 2D picking / hit test 基盤
- layout / anchor の最小仕様
- Scene / Prefab / Undo と自然につながる editor workflow

## ゴール

- Scene View を 2D 表示へ切り替えられる
- 2D 用カメラと座標系で entity を配置できる
- 2D entity をクリック選択 / 移動 / 回転 / 拡縮できる
- 2D 用の整列・スナップ・重なり順を扱える
- Scene 保存 / Load / Prefab / Undo が 2D entity に対しても動く

## 非ゴール

- UMG/Unity Canvas 完全互換 UI システム
- 9-slice, rich text, anchor/stretch のフル実装
- 2D 物理の本格導入
- Spine 等の専用 2D アニメーションシステム

## モード設計

### モード種別

- Scene View に `3D / 2D` トグルを追加
- 現在の mode は editor state に保持する
- scene ファイルには mode を保存してよいが、entity 自体は mode 専用に分断しない

### 2D Mode の表示定義

- カメラは **Orthographic**
- View 軸は固定
  - `X`: 右
  - `Y`: 上
  - `Z`: 前後/重なり順
- 2D Mode では Scene View orbit は無効
- パン / ズーム中心の操作にする

## ECS 方針

### 既存 component の再利用

- `TransformComponent`
  - 位置: `x, y` を主に使用
  - `z` は 2D では **layer / draw order 補助値**
- `NameComponent`
- `HierarchyComponent`
- `PrefabInstanceComponent`

### 新規 component

#### `SpriteComponent`

- テクスチャパス
- tint color
- size
- pivot
- draw order
- visible
- optional material

#### `CanvasItemComponent`

- 2D 編集対象であることを明示
- sorting layer
- order in layer
- visible
- active
- interactable flag
- pixel snap flag
- lock aspect

#### `Camera2DComponent` または `OrthographicCameraComponent`

- orthographic size
- zoom
- near/far
- background color

初期版では `CameraComponent` 拡張でも可。ただし将来の責務分離を考えると 2D 用 component を分ける方が望ましい。

#### `TextComponent`

- text string
- font asset
- font size
- color
- alignment
- line spacing
- wrapping

font 参照は初期版では **asset path** を正とする。
将来 asset handle 化してもよいが、scene serializer / prefab / diff の安定性を優先して、まずは文字列 path 保存で統一する。

#### `RectTransformComponent`

- anchored position
- size delta
- anchor min/max
- pivot
- rotationZ
- scale2D

`TransformComponent` を流用し切るのではなく、2D/UI authoring では `RectTransformComponent` を正式採用する。

### `TransformComponent` と `RectTransformComponent` の関係

- 2D/UI entity の **編集上の正** は `RectTransformComponent`
- `TransformComponent` は
  - 描画
  - 共通 hierarchy
  - renderer / physics / generic ECS integration
  のために必要なら保持してよい
- ただし editor は `TransformComponent` を直接編集しない
- editor は常に `RectTransformComponent` を編集し、runtime 側の `TransformComponent` はそこから導出・同期する
- つまり 2D/UI entity では
  - authoring source of truth = `RectTransformComponent`
  - runtime shared transform = `TransformComponent`
  という関係に固定する

## 新規 manager / service

### `FontManager`

2D Mode では `FontManager` を必須とする。

責務:

- font asset 読み込み
- glyph atlas 管理
- font fallback
- text measurement
- editor / runtime 共通の文字描画入口

最低限必要な API:

- `LoadFont(path, size)`
- `MeasureText(font, text, size, wrapWidth)`
- `GetGlyphAtlas(font)`

### `UIHitTestSystem`

2D 用当たり判定は専用クラスを用意する。PhysicsManager に無理に寄せない。

責務:

- screen point -> UI entity hit
- z/order を考慮した topmost hit 判定
- rect / sprite / text の hit test
- visible / active / interactable を考慮

理由:

- 2D UI の picking は物理 raycast よりも
  - rect
  - draw order
  - alpha 近似
  - anchor/layout
  を前提にした方が自然

初期版クラス名案:

- `UIHitTestSystem`
- `UIRaycast2D`
- `UICanvasPickingSystem`

## 座標系

### ワールド座標

- 2D Mode でも scene はワールド空間上に存在する
- 初期版では 1 unit = 1 editor world unit
- タイトル画面用途では pixel-perfect よりも **編集容易性** を優先

### Z の扱い

- 2D Mode では `Transform.localPosition.z` を描画優先度補助として使う
- 見た目の順序は **基本的に `sorting layer + order in layer`** で決める
- `z` は補助値、特殊用途、または同順位時の追加情報としてのみ使う
- 初期版の安定ソートは
  1. `sorting layer`
  2. `order in layer`
  3. `z`
  4. entity id
  の順とする
- UI authoring では `z` ではなく `sorting layer / order in layer` を主に触る運用を推奨する

## Scene View 操作

### カメラ操作

- 中ドラッグ: pan
- マウスホイール: zoom
- `F`: primary selection にフォーカス
- 2D Mode では Front 固定に近い操作とし、3D 視点ボタンは隠す

### Gizmo

- `ImGuizmo` を継続利用
- 2D Mode の gizmo は以下に制限する
  - Move: `X/Y` 平面のみ
  - Rotate: `Z` 軸回転のみ
  - Scale: `X/Y` のみ
- 2D Mode では `Local / World` の扱いを残すが、回転は常に Z 基準で安定化する

### クリック選択

- Scene View 左クリックで選択
- 2D Mode では picking を **`UIHitTestSystem` 基準** にする
- `PhysicsManager` は 3D scene picking 用に維持し、2D UI picking と混ぜない
- gizmo 使用中は picking 無効

### hit test 優先順

1. visible = true
2. active = true
3. interactable = true の entity 優先
4. sorting layer
5. order in layer
6. z
7. entity id

## 生成導線

### Hierarchy 右クリック

追加項目:

- Create Empty (2D)
- Sprite
- Camera (2D)
- Text (将来予約。初期版は placeholder 可)

### Asset Browser からの配置

- 画像ファイルを Scene View にドラッグすると `SpriteComponent` entity を生成
- `.prefab` は既存どおり instance 化
- 配置 preview を出す
- Drag 開始から Drop 確定まで 1 undo transaction

## Inspector

### 2D entity 用表示

- Sprite texture
- tint
- size
- pivot
- anchor
- sorting layer
- order in layer
- visible
- active
- interactable

### Text 用表示

- text
- font
- font size
- color
- alignment
- wrapping

### 2D camera 用表示

- orthographic size
- zoom
- background color

## UI 要件

### Scene View toolbar

2D Mode 時は以下を表示:

- `2D / 3D` 切替ボタン
- Move / Rotate / Scale
- Local / World
- Snap
- Zoom 表示

3D 専用 UI は隠す:

- Front/Back/Left/Right/Top/Bottom
- 3D camera speed UI

### アイコン利用

2D Mode で使うべきタイミング:

- mode 切替: `cube / image` 系 icon
- Sprite 作成: image icon
- Camera2D: camera icon
- sorting/visible: layer / eye icon

## Undo / Redo

- 既存 ECS undo をそのまま使う
- 以下を 1 transaction 単位で扱う
  - gizmo drag
  - asset drag & drop placement
  - sorting order 変更
  - duplicate
  - delete

## Scene 保存

- `.scene` にそのまま保存する
- 2D 用 entity も通常 entity と同じ scene serializer に乗せる
- mode 情報を scene metadata に保存してよい

## Prefab

- `SpriteComponent` / `CanvasItemComponent` を prefab 保存対象にする
- prefab instance は 3D と同じく ECS snapshot ベースで複製・復元する

## 描画方針

### 初期版

- 既存 renderer に 2D sprite pass を追加する
- deferred 本流へ無理に混ぜず、title/editor 用のシンプルな 2D pass とする
- alpha blending を前提にする
- text は `FontManager` 管理の atlas を使う
- sprite / text / simple panel を同じ 2D pass 群で扱えるようにする

### ソート

- 2D sprite は back-to-front ではなく、明示 order ベースの安定ソートを優先

## 実装順

### Phase 1

- ECS 2D component 基盤
- `RectTransformComponent`
- `SpriteComponent`
- 2D / 3D mode 切替
- orthographic camera
- Scene View 2D pan / zoom

### Phase 2

- `UIHitTestSystem`
- 2D picking
- 2D gizmo 制限
- Inspector 対応
- 画像 drag & drop 配置

### Phase 3

- `FontManager`
- `TextComponent`
- sorting layer / order in layer
- prefab 対応
- save/load 対応

### Phase 4

- pure ECS 2D runtime 描画の仕上げ
- editor / game view の 2D 表示整合
- UI polish
- title scene 向け最低限テンプレート
- autosave/recovery と 2D scene の整合確認

## 完了条件

- 2D Mode へ切り替えできる
- 画像を Scene View へ置ける
- text entity を置ける
- 2D entity を選択・移動・回転・拡縮できる
- sorting order を編集できる
- scene 保存 / load できる
- prefab 化 / instance 化できる
- undo / redo が壊れない
- font と 2D hit test の専用基盤が入っている

## 備考

- 初期版の目的は「タイトル画面制作に困らないこと」
- まずは高機能 UI システムではなく、**2D レイアウト編集ができる Level Editor** を作る

