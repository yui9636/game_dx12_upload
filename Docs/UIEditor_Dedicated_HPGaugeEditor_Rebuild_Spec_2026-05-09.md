# Dedicated UI Editor / HP Gauge Authoring Rebuild Spec

Created: 2026-05-09

Status: Approved direction rewrite

Supersedes:

- `Docs/UIEditor_Canvas_Authoring_Spec_2026-05-09.md`
- Current `HP Gauge Builder` dock-window implementation idea

## 1. Purpose

この仕様書は、HPゲージ作成機能を「Inspectorに押し込めた補助機能」ではなく、独立したUI作成エディターとして作り直すための設計を定義する。

今回の完成対象はHPゲージだけに限定する。  
ただし、作り方は将来的にスタミナゲージ、ロックオンUI、メニューUI、ボタンUIへ拡張できるUIエディター構造にする。

## 2. Critical Correction

前案の問題:

- HP Gauge BuilderをInspector横の小さなDock Windowとして扱っていた。
- UI作成の主操作が「エディター」ではなく「Inspector拡張」に見える。
- SceneView、Hierarchy、Inspectorに既存機能を散らして、UIを作る体験がまとまっていない。
- UnityのUI DesignerやUnreal UMG Designerに相当する「専用編集画面」がない。

修正方針:

- `UI Editor` を独立したワークスペースとして追加する。
- `Player Editor` や `Effect Editor` と同じ階層の専用タブにする。
- Inspectorは補助的なECS詳細確認に留める。
- HPゲージの作成、配置、選択、プロパティ編集、プレビュー、Prefab化はすべてUI Editor内で完結する。

禁止事項:

- HPゲージ作成の主要UIをInspectorへ置かない。
- `Window > Inspector` の内部にHPゲージ作成ボタンを置かない。
- 右端の汎用Inspectorだけでテンプレート、部品作成、Prefab操作を完結させない。
- UI Editorを単なるDock Window追加で済ませない。

## 3. Completion Scope

今回の完成条件はPhase 1からPhase 3まで。

- Phase 1: Dedicated UI Editor Workspace
- Phase 2: HP Gauge Authoring
- Phase 3: Prefab Workflow

Phase 4以降の汎用Widget Blueprint化、アニメーション、Auto Layout、Rich Text、複雑なメニュー作成は今回対象外。

## 4. User-Facing Result

ユーザーは以下の流れでHPゲージを作れる。

1. `Window > UI Editor` を開く。
2. 上部Workspace Tabで `UI Editor` に入る。
3. UI Editor内の専用作成パネルでCanvas、Image、Fill、Textを自由に組む。
4. 必要なら `Template` ボタンから `Player HP` / `Boss HP` を選び、初期形を一発作成する。
5. 中央Designer View上で移動、リサイズ、アンカー調整を行う。
6. 右のPropertiesでTarget、Fill、Text、Preview値を編集する。
7. 下部Prefab Barから `Save as HP Gauge Prefab` を押す。
8. 保存済みPrefabをScene内の好きなCanvasへ配置する。

重要:

- テンプレートボタンを押すだけで最低限ゲームで使えるHPゲージが完成する。
- 細かい部品を組むためのParts Paletteも必ず用意する。
- テンプレートは補助機能であり、部品作成機能の代替ではない。
- 主操作は「専用パネルで自由に作る」こと。
- 完成したUIはPrefab化し、Sceneへ配置して使う。
- テンプレートは最初から全種類を並べず、まず `Template` ボタンを押し、その後 `Player HP` / `Boss HP` を選択する。
- 今回のテンプレート対象は `Player HP` と `Boss HP` のみ。`Enemy HP` は今回作らない。

## 4.1 Core Workflow

今回の最重要ワークフローは以下。

```text
UI Editorを開く
  -> 専用パネルでHPゲージを自由作成
  -> Designer Viewで見た目を調整
  -> PropertiesでBindingと表示を調整
  -> Widget Prefabとして保存
  -> Scene / CanvasへPrefabを配置
  -> RuntimeでHealthComponentに接続
```

この流れが成立して初めて完成とする。  
テンプレート作成だけ、Inspector編集だけ、Sceneに直置きだけでは完成扱いにしない。

## 4.2 Free Authoring Requirement

UI Editorには `HP Gauge Authoring Panel` を置く。

この専用パネルでは、完成済みテンプレートだけではなく、以下を自由に追加、組み替え、調整できる。

- Canvas
- Gauge Root
- Background Image
- Fill Image
- Damage Preview Fill
- HP Text
- Label Text
- Decorative Image

自由作成で必要な操作:

- 部品を追加する。
- 部品を選択する。
- 部品を親子付けする。
- 部品の描画順を変える。
- 部品を移動、リサイズする。
- Texture、Color、Font、Text Formatを変える。
- Fill ImageをHP Gauge Rootへ接続する。
- HP TextをHP Gauge Rootへ接続する。
- 完成したRootをWidget Prefabとして保存する。

テンプレートボタンは、この自由作成パネル上で「よく使う構成を自動で組むショートカット」として扱う。

## 5. Editor Layout

UI Editorは専用ワークスペースとして表示する。

```text
+--------------------------------------------------------------------------------+
| Main Menu / Workspace Tabs                                                     |
| [Level Editor] [Player Editor] [Effect Editor] [UI Editor]                     |
+--------------------------------------------------------------------------------+
| UI Editor Toolbar                                                              |
| Canvas: BattleHUD_Canvas | Resolution: 1920x1080 | Snap | Safe Area | Preview |
+------------------------+---------------------------------------+---------------+
| Palette                | Designer View                         | Properties    |
|                        |                                       |               |
| Templates              | 2D Canvas                             | Selection     |
| [Template...]          |                                       | Layout        |
|   - Player HP          |    +-----------------------------+    | Binding       |
|   - Boss HP            |    | Player HP Gauge             |    | Visual        |
|                        |    +-----------------------------+    | Text          |
| Parts                  |                                       | Preview       |
| [Canvas]               |    Safe Area / Anchor Gizmos          |               |
| [Gauge Root]           |                                       |               |
| [Image]                |                                       |               |
| [Fill Image]           |                                       |               |
| [Damage Preview]       |                                       |               |
| [HP Text]              |                                       |               |
+------------------------+---------------------------------------+---------------+
| Widget Tree / Prefab Bar / Status                                              |
| BattleHUD_Canvas > PlayerHP_Widget > Background / Fill / DamagePreview / Text |
| [Save as HP Gauge Prefab] [Apply] [Revert] [Unpack] [Instantiate Prefab]       |
+--------------------------------------------------------------------------------+
```

初期版の配置は、3Dビューに重なるHUD配置として扱う。  
自由な画面内ドラッグは可能にするが、完成条件としては以下のような画面端配置ができれば十分。

- 左上
- 右上
- 上中央
- 左端
- 右端
- 下左
- 下右

複雑なAuto Layoutやレスポンシブルールは今回の完成条件に含めない。

### 5.1 Palette Panel

左側に配置する。  
役割は作成コマンドの入口。

Sections:

- Canvas
- Templates
- Parts
- Saved Prefabs

Template entry:

- `Template...`

Template chooser:

- Player HP
- Boss HP

Template interaction:

1. Paletteの `Template...` を押す。
2. 小さな選択Popup、Menu、またはSub Panelを開く。
3. `Player HP` / `Boss HP` のどちらかを選ぶ。
4. 選んだ種類の完成済みHP GaugeをActive Canvasへ作成する。

今回、`Enemy HP` テンプレートは作らない。

Parts buttons:

- Canvas
- Empty Gauge Root
- Image
- Fill Image
- Damage Preview
- HP Text

Saved Prefabs:

- `Data/UI/Prefabs/*.prefab` を列挙する。
- クリックで現在のCanvasへInstantiateする。
- ドラッグアンドドロップでDesigner Viewへ配置できるとなお良い。

### 5.2 Designer View

中央に配置する。  
ここがUI Editorの主役。

Designer Viewの責務:

- 2D Canvasの描画
- HPゲージの直接選択
- Move / Resize / Pivot / Anchorの編集
- Safe Area表示
- Grid表示
- Pixel Snap表示
- Preview Resolution切り替え
- Canvas外領域の暗転

Designer Viewは既存SceneViewの2D表示を流用してよい。  
ただし、ユーザー体験としては `Scene Viewにたまたま右パネルが付いたもの` ではなく、`UI Editorの中央Designer` として扱う。

初期版では、3D View / Game View上に表示されるHUDの配置確認を優先する。  
つまり、HPゲージが3Dビューの左上、右上、右端などに重なる形で見えることを重視する。

Designer Viewの配置プリセット:

```text
Top Left
Top Center
Top Right
Middle Left
Middle Right
Bottom Left
Bottom Right
Custom
```

プリセットを選ぶと `RectTransformComponent` のAnchor、Pivot、Position初期値を更新する。  
CustomではDesigner View上でドラッグして微調整できる。

### 5.3 Widget Tree

下部または左下に配置する。  
HierarchyのUI専用版。

表示対象:

```text
BattleHUD_Canvas
  PlayerHP_Widget
    Background
    DamagePreview
    Fill
    HP_Text
  BossHP_Widget
    Background
    DamagePreview
    Fill
    HP_Text
```

Widget Treeの役割:

- UI関連Entityだけを表示する。
- Scene全体の汎用Hierarchyを見せない。
- HP Gauge Rootと子要素を選択できる。
- Renameできる。
- 子要素の順序を変更できる。
- Delete / Duplicateできる。

### 5.4 Properties Panel

右側に配置する。  
これはECS Inspectorではなく、UI Editor専用のPropertiesである。

表示内容は選択対象に応じて切り替える。

HP Gauge Root選択時:

- Name
- Target Mode
- Explicit Target
- Visible When No Target
- Hide When Dead
- Hide When Full
- Smoothing Speed
- Damage Preview Delay
- Layout
- Preview HP
- Prefab状態

Fill選択時:

- Texture
- Tint
- Fill Direction
- Color Mode
- High / Mid / Low Color
- Threshold
- Runtime Preview Ratio

Text選択時:

- Text Format
- Label
- Font
- Font Size
- Color
- Alignment

Image選択時:

- Texture
- Tint
- Order
- Visible

重要:

- ECS Inspectorのコンポーネント一覧をそのまま見せない。
- UI作成に必要な項目へ整理して表示する。
- 内部的には既存Componentを編集してよい。
- 操作はUndo / Redo対象にする。

## 6. Workspace Integration

`EditorLayer::WorkspaceTab` に `UIEditor` を追加する。

```cpp
enum class WorkspaceTab
{
    LevelEditor,
    PlayerEditor,
    EffectEditor,
    UIEditor
};
```

追加する状態:

```cpp
bool m_showUIEditor = false;
UIEditorPanel m_uiEditorPanel;
```

メニュー:

```text
Window > UI Editor
```

表示ルール:

- `UI Editor` が開いているとWorkspace Tabに表示する。
- Tab選択時はLevel Editorの通常DockSpaceではなく `DrawUIEditorWorkspace()` を描画する。
- Player Editor / Effect Editorと同じ思想で、UI Editorも専用ワークスペースとして扱う。

削除または無効化するもの:

- `HP Gauge Builder` 単体Dock Window
- Inspector横に配置されたHPゲージ作成パネル
- 汎用Windowメニュー内の `HP Gauge Builder`

## 7. Proposed Files

新規追加:

```text
Source/UIEditor/UIEditorPanel.h
Source/UIEditor/UIEditorPanel.cpp
Source/UIEditor/UIEditorState.h
Source/UIEditor/UIEditorPalettePanel.cpp
Source/UIEditor/UIEditorDesignerView.cpp
Source/UIEditor/UIEditorWidgetTree.cpp
Source/UIEditor/UIEditorPropertiesPanel.cpp
Source/UIEditor/UIEditorPrefabPanel.cpp
Source/UIEditor/HPGaugeTemplateFactory.h
Source/UIEditor/HPGaugeTemplateFactory.cpp
```

既存EditorLayer側の追加:

```text
Source/Layer/EditorLayer.h
Source/Layer/EditorLayer.cpp
Source/Layer/EditorLayerMenu.cpp
```

既存の削除候補:

```text
Source/Layer/EditorLayerHPGaugeBuilder.cpp
```

注意:

- `EditorLayerHPGaugeBuilder.cpp` のロジックは捨てるのではなく、`HPGaugeTemplateFactory` と `UIEditorPrefabPanel` へ分解して移す。
- 表示場所だけを移すのではなく、UI Editor全体の操作体験へ組み直す。

## 8. Data Model

初期実装では新しい汎用UI Componentを大量追加しない。  
既存Componentを使い、UI Editor側で作成体験を整える。

使用する既存Component:

- `NameComponent`
- `HierarchyComponent`
- `TransformComponent`
- `RectTransformComponent`
- `CanvasItemComponent`
- `SpriteComponent`
- `TextComponent`
- `HPGaugeBindingComponent`
- `HPGaugeFillComponent`
- `HPGaugeTextComponent`
- `PrefabInstanceComponent`

HP Gauge構造:

```text
BattleHUD_Canvas
  PlayerHP_Widget
    Background
    DamagePreview
    Fill
    HP_Text
```

Root:

- `NameComponent`
- `HierarchyComponent`
- `TransformComponent`
- `RectTransformComponent`
- `CanvasItemComponent`
- `HPGaugeBindingComponent`

Background:

- `SpriteComponent`

DamagePreview:

- `SpriteComponent`
- `HPGaugeFillComponent`
  - `useDisplayedRatio = false`
  - `useDelayedRatio = true`

Fill:

- `SpriteComponent`
- `HPGaugeFillComponent`
  - `useDisplayedRatio = true`
  - `useDelayedRatio = false`

HP_Text:

- `TextComponent`
- `HPGaugeTextComponent`

## 9. Template Creation

### 9.1 Player HP Template

Default:

```text
Name: PlayerHP_Widget
Target Mode: FirstPlayer
Anchor: Top Left
Position: 48, -48
Size: 440 x 56
Fill Direction: LeftToRight
Text Format: Current / Max
```

### 9.2 Boss HP Template

Default:

```text
Name: BossHP_Widget
Target Mode: FirstBoss
Anchor: Top Center
Position: 0, -42
Size: 760 x 46
Fill Direction: LeftToRight
Text Format: Label + Current / Max
Label: BOSS
```

### 9.3 Creation Behavior

`Template...` ボタンを押して種類を選んだ時:

1. `Template...` ボタンで選択Popup、Menu、またはSub Panelを開く。
2. `Player HP` / `Boss HP` のどちらかを選ぶ。
3. Active Canvasがなければ `BattleHUD_Canvas` を作る。
4. Active Canvasの子にHP Gauge Rootを作る。
5. Background / DamagePreview / Fill / HP_Textを子として作る。
6. Rootを選択する。
7. Designer ViewをRootが見える位置へフォーカスする。
8. Properties PanelにRoot編集項目を表示する。
9. Undo Stackに `Create HP Gauge Template` として登録する。

## 10. Parts Creation

Parts Paletteは独自構成を作るための入口。

Canvas:

- `BattleHUD_Canvas` を作成または選択する。

Empty Gauge Root:

- `HPGaugeBindingComponent` を持つRootだけを作る。
- 子要素は作らない。

Image:

- 選択中HP Gauge Rootの子に通常Imageを作る。
- Root未選択時はActive Canvas直下に作る。

Fill Image:

- 選択中HP Gauge Rootの子に作る。
- `HPGaugeFillComponent` を付ける。
- Rootがない場合は警告を出す。

Damage Preview:

- 選択中HP Gauge Rootの子に作る。
- `HPGaugeFillComponent(useDelayedRatio = true)` を付ける。
- Rootがない場合は警告を出す。

HP Text:

- 選択中HP Gauge Rootの子に作る。
- `TextComponent` と `HPGaugeTextComponent` を付ける。
- Rootがない場合は警告を出す。

## 11. Binding UX

Properties PanelのBinding欄:

```text
Target Mode
  Explicit
  First Player
  First Boss

Explicit Target
  [Pick From Scene Selection]
  [Clear]

Behavior
  [x] Visible When No Target
  [x] Hide When Dead
  [ ] Hide When Full

Smoothing
  Displayed Fill Speed
  Damage Preview Delay
  Damage Preview Speed
```

Explicit Targetの指定:

- Scene内でHealthComponentを持つEntityを選択する。
- `Pick From Scene Selection` でRootの `explicitTarget` に設定する。
- UI Editorは選択中Gauge Rootを保持しているため、ターゲットEntityを選択しても編集対象Gaugeを見失わない。

## 12. Preview UX

Preview欄:

```text
Preview HP
  Current: [ 75 ]
  Max:     [100 ]
  [100%] [75%] [50%] [25%] [0%]
  [Animate Damage Preview]
```

PreviewはEditor専用値として扱う。

Runtime中:

- `HPGaugeSystem` がHealthComponentから実値を読む。

Editor中:

- Targetが見つかる場合はTarget値を表示する。
- Targetがない場合でもPreview HPでFillとTextを確認できる。

## 12.1 Placement MVP

配置機能の初期完成ラインは「3Dビュー上のHUDとして使える位置へ置けること」。

必要な配置操作:

- Presetから左上に置く。
- Presetから右上に置く。
- Presetから右端に置く。
- Presetから上中央に置く。
- PositionのX/Yで少しずらす。
- Sizeで横幅と高さを調整する。

不要なもの:

- 複雑なConstraint Solver
- Auto Layout
- Content Size Fitter
- Layout Group
- 複数解像度ごとの個別レイアウト保存
- World Space UI

このMVPでは、3D View / Game Viewに対して「画面端にHPゲージが置ける」ことを優先する。

## 13. Prefab Workflow

保存先:

```text
Data/UI/Prefabs/
```

UI Editor内のPrefab Bar:

```text
[Save as HP Gauge Prefab] [Apply] [Revert] [Unpack]
```

Saved Prefabs Palette:

```text
Data/UI/Prefabs/
  PlayerHP_Widget.prefab
  BossHP_Widget.prefab
```

### 13.1 Save

`Save as HP Gauge Prefab`:

1. 選択中HP Gauge RootのSubtreeを保存する。
2. `PrefabSystem::SaveEntityAsPrefab` を呼ぶ。
3. 保存後、Rootに `PrefabInstanceComponent` を付ける。
4. Prefab Barに保存先を表示する。

### 13.2 Instantiate

Saved Prefabsの項目をクリック:

1. Active Canvasを確認する。
2. なければCanvasを作る。
3. `PrefabSystem::InstantiatePrefab` を呼ぶ。
4. 作成Rootを選択する。
5. Designer Viewへフォーカスする。

### 13.3 Apply

`Apply`:

1. Rootの現在SubtreeをPrefab元へ上書きする。
2. `hasOverrides = false` にする。
3. Undo対象にする。

### 13.4 Revert

`Revert`:

1. Prefab元を読み込む。
2. 現在Root Subtreeを置き換える。
3. 選択Rootを差し替え後Rootに更新する。
4. Undo対象にする。

### 13.5 Unpack

`Unpack`:

1. `PrefabInstanceComponent` だけを外す。
2. Entity構造は残す。
3. Undo対象にする。

## 14. Undo / Redo

Undo対象:

- Canvas作成
- Template作成
- Parts作成
- Move
- Resize
- Rename
- Parent変更
- Order変更
- Component値変更
- Save後のPrefabInstanceComponent付与
- Apply
- Revert
- Unpack
- Instantiate

方針:

- 既存 `UndoSystem` を使う。
- Entity追加は `CreateEntityAction`。
- Subtree置換は `ReplaceEntitySubtreeAction`。
- Component変更は `ComponentUndoAction`。
- Optional Component変更は `OptionalComponentUndoAction`。

## 15. Runtime Behavior

Runtimeは既存のHPGauge実行系を使う。

使用するSystem:

- `HPGaugeSystem`
- `UI2DSpriteExtractSystem`
- `UI2DSpriteRenderSystem`
- `UI2DTextExtractSystem`
- `UI2DTextRenderSystem`

UI EditorはRuntime用の特別な描画経路を増やさない。

Runtime更新:

1. `HPGaugeBindingComponent` がHealthComponentを解決する。
2. `HPGaugeFillComponent` がFill ratioを受ける。
3. SpriteのTintとFill clippingが更新される。
4. `HPGaugeTextComponent` がTextを更新する。

## 16. Implementation Phases

### Phase 1: Dedicated UI Editor Workspace

実装内容:

- `WorkspaceTab::UIEditor` を追加。
- `UIEditorPanel` を追加。
- `Window > UI Editor` を追加。
- Level EditorのDockSpaceとは別の専用UI Editor layoutを描画。
- Palette / Designer View / Widget Tree / Properties / Prefab Barを表示。
- 既存 `HP Gauge Builder` Dock Windowを削除または無効化。

完了条件:

- UI EditorがInspectorとは別のワークスペースとして開く。
- UI Editor内に専用PaletteとDesigner Viewが存在する。
- Inspectorを開かなくてもHPゲージ作成の入口が見える。

### Phase 2: HP Gauge Authoring

実装内容:

- `Template...` ボタンから `Player HP` / `Boss HP` を選択してテンプレート作成。
- Canvas / Root / Image / Fill / DamagePreview / Text部品作成。
- Designer Viewで選択、移動、リサイズ。
- Widget TreeでUI Subtree選択。
- Properties PanelでHP Gauge Root / Fill / Text / Imageを編集。
- Preview HP操作。
- 3D View / Game Viewの左上、右上、右端、上中央に配置できるPlacement Preset。

完了条件:

- テンプレートボタンだけで完成済みHPゲージが作れる。
- 部品ボタンだけでも同等構造を手動構築できる。
- 作成後にScene保存、読み込みで復元する。
- RuntimeでHealthComponentに追従する。

### Phase 3: Prefab Workflow

実装内容:

- Save as HP Gauge Prefab。
- Saved Prefabs Palette。
- Instantiate。
- Apply。
- Revert。
- Unpack。
- Undo / Redo。

完了条件:

- 作ったHP GaugeをPrefab化できる。
- 保存Prefabを別Sceneまたは別Canvasに再配置できる。
- Prefab編集後にApply / Revert / Unpackできる。
- Inspectorを使わずUI Editor内でPrefab操作が完結する。

## 17. Acceptance Checklist

UI Editor:

- `Window > UI Editor` で専用ワークスペースが開く。
- Workspace Tabに `UI Editor` が表示される。
- Inspectorを閉じていてもHPゲージ作成ができる。
- UI Editor内にPalette、Designer View、Widget Tree、Properties、Prefab Barがある。

HP Gauge:

- `Template...` ボタンを押すとテンプレート選択UIが開く。
- `Player HP` テンプレートが作れる。
- `Boss HP` テンプレートが作れる。
- Canvas、Root、Image、Fill、DamagePreview、Textを個別作成できる。
- Designer View上で移動、リサイズできる。
- Placement Presetで画面左上、右上、右端、上中央に置ける。
- Widget Treeで子要素を選択できる。
- PropertiesでTarget、Fill、Text、Previewを編集できる。

Prefab:

- `Save as HP Gauge Prefab` で `Data/UI/Prefabs` に保存される。
- 保存済みPrefabがPaletteに表示される。
- PaletteからPrefabを再配置できる。
- Applyできる。
- Revertできる。
- Unpackできる。

Runtime:

- HealthComponentのHP変化がFillに反映される。
- Textが現在HP / 最大HPを表示する。
- DamagePreviewが遅延Fillとして動く。

## 18. Rework Instructions

現在入っている実装から作り直す場合の順序:

1. `EditorLayerHPGaugeBuilder.cpp` を廃止対象にする。
2. そこにあるテンプレート生成関数だけを `HPGaugeTemplateFactory` へ移す。
3. `EditorLayer` の `m_showHPGaugeBuilder` と `WindowFocusTarget::HPGaugeBuilder` を消す。
4. `WorkspaceTab::UIEditor` と `m_showUIEditor` を追加する。
5. `UIEditorPanel` を作る。
6. UI Editor内にPalette / Designer / Widget Tree / Properties / Prefab Barを実装する。
7. Phase 1の見た目が完成してから、Phase 2のテンプレート作成を移植する。
8. Phase 2が通ってから、Phase 3のPrefab操作を移植する。

この順番を守る。  
先に便利ボタンだけ増やすと、またInspector拡張に戻ってしまう。

## 19. Final Direction

HPゲージ作成は「Inspectorでコンポーネントを編集する機能」ではない。  
これは「UIを作るための専用エディター」である。

今回の最小完成形はHPゲージだけでよい。  
しかし、ユーザーが触る画面は最初からUI Editorとして成立していなければならない。
