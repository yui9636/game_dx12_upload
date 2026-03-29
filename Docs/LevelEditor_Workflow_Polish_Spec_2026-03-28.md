# レベルエディター運用改善仕様書

更新日: 2026-03-28

## 目的

- レベルエディターの現在の運用上の不便を解消する
- 「正しく切り替わる環境設定」「軽い初期シーン」「活用できる Edit/Game View」を整える
- 今後の 3D/2D authoring の土台として、scene の初期状態と view の責務を明確にする

## 対象

今回の対象は以下の 3 点とする。

1. skybox on/off が現在機能しない問題
2. 初期シーンで自動生成される `Actor (A5)` を外す
3. `Edit View Window`（現行 `Game View` を含む view 窓）の活用強化

## 現状の問題

### 1. skybox on/off が効かない

現状は環境設定の source of truth が二重化している。

- `GameLayer::m_environment`
- ECS の `EnvironmentComponent`

現状コードでは:

- `EditorLayer` の Lighting Settings は `GameLayer::GetEnvironment()` を直接編集している
- 一方で scene 保存/読込は `EnvironmentComponent` に乗っている
- さらに render 時に `GameLayer::Render()` が `rc.environment` を毎フレーム上書きしている

結果として、

- inspector / scene 上の environment 設定
- runtime render に渡る environment 設定

の間で整合が崩れ、`enableSkybox` の on/off が正しく反映されない。

### 2. 初期シーンの A5 自動生成

現状は editor 起動直後や new scene 時に、初期 entity として `Actor (A5)` が自動で入る箇所がある。

- `GameLayer::Initialize()`
- `EditorLayer::CreateDefaultSceneEntities(...)`

これは

- 軽い empty scene を作りたいケース
- UI scene / title scene 制作
- 2D mode

では不要であり、初期状態として重すぎる。

### 3. Edit View Window の活用不足

現状の view 窓は

- `Scene View`
- `Game View`

があるが、役割分担が弱い。

問題:

- `Scene View` は authoring 用として機能している
- `Game View` は単なる render 表示に近く、制作上の意味が薄い
- 2D mode 時の確認窓としても用途が整理されていない

## 基本方針

### 1. environment の正を ECS に一本化する

- environment の正式な source of truth は **ECS の `EnvironmentComponent`**
- `GameLayer::m_environment` は廃止、または単なる一時キャッシュに格下げする
- editor / scene save/load / runtime render はすべて `EnvironmentComponent` を基準にする

### 2. default scene は最小構成にする

初期 scene / new scene の標準構成は以下とする。

- `Main Camera`
- `Directional Light`
- `Reflection Probe`

`Actor (A5)` は自動生成しない。

必要なら user が prefab / asset から明示的に配置する。

### 3. Edit/Game View の役割を分ける

- `Scene View`
  - authoring / selection / gizmo / placement
- `Game View`
  - runtime preview / 解像度確認 / UI確認 / play 寄り確認

「単なる同じ映像の二枚表示」にしない。

## 詳細仕様

### A. Skybox on/off 修正

#### source of truth

- `EnvironmentComponent.enableSkybox`
- `EnvironmentComponent.skyboxPath`
- `EnvironmentComponent.diffuseIBLPath`
- `EnvironmentComponent.specularIBLPath`

のみを正式値とする。

#### editor UI

- Lighting Settings / Environment セクションは、`GameLayer::m_environment` ではなく
  **scene 内の `EnvironmentComponent` entity** を直接編集する
- scene 内に environment entity が無ければ、editor 側で 1 つ保証してよい

#### render

- `GameLayer::Render()` が `rc.environment` を直接上書きする構造はやめる
- 環境値の収集は `EnvironmentExtractSystem` に統一する

#### save/load

- scene 保存/読込の対象は `EnvironmentComponent` のみ
- 起動直後の default scene でも `EnvironmentComponent` を持つ entity を必ず 1 つ作る

#### 完了条件

- `enableSkybox=false` で skybox が完全に消える
- `enableSkybox=true` で戻る
- save/load 後も状態が維持される

### B. 初期 A5 自動生成の廃止

#### 対象

- `GameLayer::Initialize()`
- `EditorLayer::CreateDefaultSceneEntities(...)`

#### 仕様

- `Actor (A5)` の自動生成コードはコメントアウトではなく削除してよい
- default scene は軽量な empty authoring scene を基準にする
- デモモデルが必要なら、将来は
  - menu から `Create Sample Scene`
  - sample prefab
  の形で明示生成する

#### 完了条件

- editor 起動直後の scene に A5 がいない
- `New Scene` 後も A5 がいない

### C. Edit View / Game View の有効活用

## 役割再定義

### `Scene View`

- 編集専用 view
- gizmo / drag drop / selection / placement / debug overlay を持つ

### `Game View`

- runtime 確認専用 view
- 選択や gizmo は持たない
- 2D/UI 制作時は **resolution preview** の役割を持つ

## 追加機能

### 1. Resolution preset

`Game View` toolbar または header に以下を持つ。

- 1920x1080
- 1280x720
- 1080x1920
- 750x1334
- free

### 2. Aspect lock

- `Game View` は preset 選択時に aspect を固定して letterbox/pillarbox 表示する
- `Scene View` は引き続き自由サイズ

### 3. Play/Edit 表示整理

- `Game View` は play 中だけでなく edit 中も preview に使える
- 2D mode では `Game View` を title screen / UI preview 窓として活用する

### 4. Overlay 切替

`Game View` には最低限以下を切替可能とする。

- UI preview on/off
- safe area on/off
- pixel preview on/off

### 5. Camera binding

- 3D mode では `Game View` は main camera ベース
- 2D mode では active `Camera2DComponent` ベース

### 6. Empty 画面の扱い

- 対応 camera が無い場合は
  - `No active camera`
  - `Create Main Camera` / `Create 2D Camera`
  の導線を出す

## UI 方針

### File / Window

- `Window` メニューに
  - `Scene View`
  - `Game View`
  の表示切替を明示

### View header

- `Game View` 側は authoring 用アイコンを置きすぎない
- 代わりに
  - resolution
  - safe area
  - preview toggles
  を置く

## 実装フェーズ

### Phase 1

- skybox source of truth を ECS に一本化
- `GameLayer::m_environment` の render 上書きを外す
- Lighting Settings を ECS `EnvironmentComponent` に接続

### Phase 2

- `Actor (A5)` 自動生成削除
- default scene の最小化

### Phase 3

- `Game View` 役割整理
- resolution preset
- aspect lock
- 2D preview overlay

## 完了条件

- skybox on/off が save/load 含めて正しく効く
- default scene に A5 が出ない
- `Game View` が runtime preview 窓として意味を持つ
- 2D/UI 制作時に `Game View` で解像度確認できる

## 備考

- 今回は「新機能追加」よりも「既存エディタの役割整理」を優先する
- 特に skybox と default scene は、今後の 2D/UI authoring の邪魔になるため優先度は高い
