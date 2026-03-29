# レベルエディター 2D Mode 強化仕様書

更新日: 2026-03-28

## 目的

- 既存の 2D Mode 初期版を、**タイトル画面 / UI 画面 / 2D 演出シーン** を本番制作できる水準まで強化する
- 2D/UI authoring を **pure ECS** で完結させる
- Unity / Unreal Engine の UI authoring 体験に近づける

## 前提

- 旧 `Source/UI` 基盤は互換維持対象ではない
- 2D/UI の正は ECS component
- editor / runtime / scene save / prefab / undo は同じ ECS データモデルを使う
- 現在の初期版で以下は導入済み
  - `2D / 3D` 切替
  - `RectTransformComponent`
  - `CanvasItemComponent`
  - `SpriteComponent`
  - `TextComponent`
  - `Camera2DComponent`
  - 2D picking
  - 2D gizmo
  - sprite / text 作成
  - scene / prefab / undo 基本導線

## 強化の基本方針

- 新しい 2D 機能はすべて **ECS component + ECS system** として追加する
- 3D editor と別エディタに分けない
- Scene View / Hierarchy / Inspector / Asset Browser / toolbar を拡張して育てる
- UI は「ランタイムでもそのまま使えるデータ」を直接編集する

## 現状の不足

- Canvas / Screen 単位の概念が弱い
- 解像度・アスペクト・safe area を前提にした authoring がない
- anchor / stretch / layout が最小限すぎる
- 2D 用の create 導線はあるが、**実制作向けの整列・配置補助** が不足
- Sprite / Text は置けるが、**Button / Panel / Image / Mask / 9-slice** などの UI 部品がない
- runtime 側 2D 描画は editor overlay 寄りで、専用 pass としては未整理
- タイトル画面制作に必要な
  - 画面遷移
  - hover/click
  - 選択状態
  - tween/簡易 animation
  が未整備

## ゴール

- タイトル画面を ECS ベースで普通に作れる
- resolution / aspect を切り替えて UI 崩れを確認できる
- panel / image / button / text を配置し、操作・遷移まで組める
- layout / alignment / distribute / anchors が実用レベルで使える
- runtime で editor と同等に 2D/UI を描画できる

## 非ゴール

- UMG 完全互換
- Unity UI Toolkit 完全互換
- 複雑な flexbox / web layout
- 本格的な 2D 物理エンジン

## 強化対象の柱

### 1. Canvas / Screen 基盤

#### 新規 component

##### `CanvasComponent`

- render mode
  - `ScreenSpace`
  - `WorldSpace`
- reference resolution
- match width/height
- safe area 使用フラグ
- background clear 色
- sort target

##### `ScreenRootComponent`

- タイトル画面や HUD などの **画面ルート** を表す
- Scene 内に複数持てる
- active screen 切替対象になる

#### ルール

- 2D/UI entity は必ずいずれかの `CanvasComponent` 配下に属する
- `CanvasComponent` を持つ entity が 2D レイアウト空間の親になる
- `ScreenRootComponent` は scene load 時の既定表示単位になる

### 2. RectTransform 強化

#### `RectTransformComponent` 強化項目

- current:
  - anchored position
  - size delta
  - anchor min/max
  - pivot
  - rotationZ
  - scale2D
- add:
  - min size
  - max size
  - stretch flags
  - keep aspect

#### ルール

- 2D/UI entity の編集上の正は常に `RectTransformComponent`
- `TransformComponent` は runtime 描画や共通 hierarchy のための同期結果
- editor は `RectTransformComponent` だけを編集する

### 3. 画面解像度 / アスペクト authoring

#### Scene View 2D toolbar に追加

- resolution preset
  - 1920x1080
  - 1280x720
  - 1080x1920
  - 750x1334
- aspect preset
  - 16:9
  - 9:16
  - 4:3
  - free
- safe area overlay toggle
- pixel preview toggle

#### 表示

- Scene View 上に
  - game frame
  - safe area
  - canvas bounds
  をガイド表示する

### 4. 2D/UI 向け component 拡張

#### `ImageComponent`

- sprite path
- color
- image type
  - simple
  - sliced
  - tiled
- preserve aspect

#### `PanelComponent`

- color
- optional sprite background
- border
- rounded corner は将来対応

#### `ButtonComponent`

- normal / hover / pressed / disabled 状態色
- interactable
- target action id
- transition type
  - color tint
  - sprite swap

#### `MaskComponent`

- rect clipping
- child clip

#### `LayoutGroupComponent`

初期版は簡易:

- horizontal
- vertical
- spacing
- padding
- child alignment

#### `ContentSizeFitterComponent`

初期版は最小:

- fit width
- fit height

### 5. Text 基盤強化

#### `TextComponent` 拡張

- rich text は未対応でよい
- add:
  - overflow mode
  - auto size
  - outline color
  - shadow color
  - shadow offset

#### font ルール

- font 参照は asset path を正とする
- runtime/editor 共通で使う `FontManager` を正式採用する
- `FontManager` の責務
  - font asset load
  - atlas build
  - text measure
  - wrap / align
  - fallback font

### 6. 2D renderer / runtime pass 正式化

#### 必要事項

- 2D renderer を editor overlay の寄せ集めではなく、**正式な runtime pass** にする
- sprite / text / panel / image を同一 2D pass 群で描けるようにする
- alpha blend
- clip rect
- stable sort

#### ソート規則

1. canvas sort order
2. sorting layer
3. order in layer
4. z
5. entity id

### 7. 2D hit test / interaction

#### `UIHitTestSystem` 強化

- panel / image / button / text / mask を対象にする
- topmost 判定
- clip / mask を考慮
- visible / active / interactable を反映

#### interaction

- hover
- pressed
- clicked
- selected
- focused

#### 新規 component

##### `UIInteractionStateComponent`

- hovered
- pressed
- focused
- disabled

### 8. タイトル画面制作向け機能

#### Screen 切替

##### `UIScreenStateComponent`

- state id
- visible
- modal
- default focus target

##### `UIScreenTransitionComponent`

- fade in/out
- slide
- scale
- duration
- easing id

#### action routing

##### `UIActionComponent`

- action id
- target scene
- target screen
- target entity
- optional sound id

### 9. editor 操作強化

#### 必須

- marquee select
- 複数選択 align
  - left
  - right
  - top
  - bottom
  - center
  - middle
- distribute
  - horizontal
  - vertical
- same width / height
- parent change
- bring forward / send backward

#### gizmo 補助

- pivot / center 切替
- local / world
- rect edge drag
- anchor presets popup

#### hierarchy

- canvas 単位の色分け
- 2D icon
- lock / hide
- filter
  - sprites
  - text
  - buttons
  - panels

### 10. Asset Browser 導線強化

- texture drag -> image/sprite
- font drag -> text
- prefab drag -> screen subtree
- multi drop
- title template prefab 作成

### 11. Undo / Prefab / Scene

#### 追加要求

- layout 操作は 1 transaction
- align / distribute は 1 transaction
- canvas resolution 変更も undo 可能
- prefab は UI subtree 全体を保持
- scene 保存時に
  - current screen
  - 2D view mode
  - resolution preset
  を保存してよい

### 12. autosave / recovery 強化

- 2D scene も通常 scene と同様に autosave
- recovery popup で scene type を表示
- autosave は scene 名ごとに複数世代保持

## UI 方針

### toolbar

- 2D mode 時は 2D 専用 toolbar を優先
- 3D 向けボタンは隠す
- 解像度 / safe area / snap / align を並べる

### icon

- canvas: layer-group / window 系
- screen root: image / desktop 系
- sprite/image: image 系
- text: font 系
- button: hand-pointer / square-check 系
- panel: square 系
- mask: crop / object-group 系

### popup / menu

- anchor presets
- align / distribute
- create 2D UI
- convert to button / panel / image

## 実装フェーズ

### Phase A: 画面基盤

- `CanvasComponent`
- `ScreenRootComponent`
- resolution / aspect / safe area overlay
- 2D scene metadata 強化

### Phase B: 2D runtime 描画正式化

- pure ECS 2D render pass
- `ImageComponent`
- `PanelComponent`
- `MaskComponent`
- `FontManager` 正式運用

### Phase C: interaction

- `ButtonComponent`
- `UIInteractionStateComponent`
- hover / click / selected
- focus system

### Phase D: editor 快適化

- marquee select
- align / distribute
- anchor preset
- hierarchy filter
- icon polish

### Phase E: タイトル画面制作機能

- `UIScreenStateComponent`
- `UIScreenTransitionComponent`
- `UIActionComponent`
- screen 切替 preview

## 完了条件

- タイトル画面の 1 画面を editor 上だけで作れる
- sprite / image / panel / text / button を置ける
- 解像度切替と safe area 確認ができる
- align / distribute / anchor が使える
- runtime でも editor と同等に表示される
- save / load / prefab / undo が壊れない
- 旧 `Source/UI` を参照しなくても 2D/UI が成立する

## 優先度

### S

- `CanvasComponent`
- resolution / safe area
- pure ECS 2D render pass
- `ImageComponent`
- `ButtonComponent`
- `FontManager` 正式化

### A

- align / distribute
- `PanelComponent`
- `MaskComponent`
- marquee select
- screen state / transition

### B

- layout group
- content size fitter
- text effect
- title template

## 備考

- 目標は「2D が触れる」ではなく、**タイトル画面制作に困らないこと**
- そのため、単なる sprite editor ではなく、**UI authoring 機能**を優先して伸ばす
