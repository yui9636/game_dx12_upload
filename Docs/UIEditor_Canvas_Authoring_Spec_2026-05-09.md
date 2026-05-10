# UI Editor Canvas Authoring Spec

Created: 2026-05-09

## 1. Purpose

HPゲージ作成を専用機能として場当たり的に増やすのではなく、Scene上でHPゲージを作成・配置・保存できる最小UI作成エディターへ昇華する。

この仕様の目的は、UnityのCanvas / RectTransform / Prefabワークフローと、Unreal EngineのUMG Designer / Widget Blueprint的な作成体験を参考にしつつ、現在のECS、SceneView、PrefabSystem、UI2D描画基盤に合う形で実装方針を決めることである。

今回の完成条件はHPゲージ作成に限定する。  
ただし、設計は将来のスタミナゲージ、ボタン、メニュー、ロックオンUIへ拡張できる形にしておく。

## 2. Goals

今回の実装ゴールはHPゲージ作成に絞る。

- SceneView上でHPゲージ用の2D Canvasを編集できる。
- Player / Enemy / Boss向けHPゲージをPaletteまたはCreate Menuから作成できる。
- HPゲージの位置、サイズ、アンカー、ピボット、表示順、フォント、色、画像、Fill方向をInspectorで編集できる。
- HPゲージの構成要素をHierarchyで扱える。
- 作成したHPゲージをSceneまたはPrefabとして保存できる。
- RuntimeではSceneに置かれたHPゲージentityをそのまま描画・更新する。
- HealthComponentとの接続はBindingとして分離する。
- GameLayerに個別HPゲージ生成コードを増やさない。

## 2.1 Initial Completion Definition

この仕様の「完成」は、汎用UIエディター全体の完成ではない。  
完成条件は以下に限定する。

- UI EditorからHP Gaugeを作成できる。
- 作成されたHP GaugeはBackground、Fill、DamagePreview、Textを持つ。
- SceneView上でHP Gauge全体と各子要素を移動・リサイズできる。
- InspectorでHP GaugeのTarget、Fill方向、色、Text形式、Preview値を編集できる。
- Player、Enemy、BossのHealthComponentに接続できる。
- Scene保存・読み込みでHP Gaugeが復元される。
- Prefabとして保存し、別Sceneに再配置できる。
- RuntimeでHP変動がFillとTextに反映される。
- 1280x720と1920x1080で意図した位置・サイズを保てる。

ボタン、メニュー、ロックオンUI、汎用Widget Blueprint的な機能は、この完成条件には含めない。

## 3. Non Goals

初期実装では以下を対象外にする。

- 完全なBlueprint / Visual Scripting
- 複雑なUIアニメーションタイムライン
- Rich Text、ルビ、縦書き、複雑な文字組み
- 高度なLocalization Table
- ScrollView、Virtualized List
- 3D空間上のWorld Space Widget
- 入れ子Prefabの完全なOverride UI
- Auto Layoutの完全再現
- Button / Menu / Lock-on UI の完成
- 汎用Widget作成ツールとしての完成

ただし、将来拡張できるように、Canvas、Widget、Binding、Templateは分離して設計する。

## 4. Current Situation

既存の2D UI基盤には以下がある。

- `RectTransformComponent`
- `CanvasItemComponent`
- `SpriteComponent`
- `TextComponent`
- `UIButtonComponent`
- `UIHitTestSystem`
- `UI2DSpriteExtractSystem`
- `UI2DSpriteRenderSystem`
- `UI2DTextExtractSystem`
- `UI2DTextRenderSystem`
- `HPGaugeBindingComponent`
- `HPGaugeFillComponent`
- `HPGaugeTextComponent`
- `PrefabSystem`

現状の弱点は、UIを「作る場所」と「ゲーム値に接続する場所」が分かれておらず、HPゲージのような機能を作るたびに専用コンポーネントと専用Inspectorが増えやすいこと。

また、SceneViewで見たまま配置する作成体験がまだ弱く、HPゲージを作るにも、複数entityの親子構造、Fill用画像、Text、Bindingを手作業で組み合わせる必要がある。

## 5. Design References

### 5.1 Unityから取り入れる点

- CanvasをUIのRootとして扱う。
- RectTransformのAnchor / Pivot / Anchored Position / Sizeを中心に配置する。
- SceneView上でRectを直接編集する。
- PrefabとしてUI部品を再利用する。
- Canvas Scalerで基準解像度とスケール方針を持つ。

### 5.2 Unreal Engine UMGから取り入れる点

- PaletteからButton、Image、Text、ProgressBarなどを追加する。
- Designer Viewで見た目を編集する。
- HierarchyでWidgetツリーを編集する。
- Details Panelで選択Widgetのプロパティを編集する。
- Bindingでゲーム値と表示を接続する。
- Preview値を入れて、Runtime前に表示確認できる。

### 5.3 このエンジンで変える点

Unity / UEのクラス構造をそのまま持ち込まない。

このエンジンでは、UIはECS entityとしてSceneに存在する。  
Widgetは「特別なオブジェクト」ではなく、`RectTransformComponent`、`CanvasItemComponent`、描画Component、Binding Componentの組み合わせとして表現する。

## 6. Core Concept

UIエディターは以下の4層で構成する。

```text
UI Canvas
  UI Widget
    Visual Component
    Layout Component
    Interaction Component
    Binding Component
```

例:

```text
BattleHUD_Canvas
  PlayerHP_Widget
    HP_Background
    HP_Fill
    HP_DamagePreview
    HP_Text
  BossHP_Widget
    Boss_Label
    Boss_Fill
  LockOn_Widget
    Reticle_Image
```

HPゲージは以下のような汎用Widgetとして扱う。

```text
ProgressBar Widget
  Background Image
  Fill Image
  Optional Delayed Fill Image
  Optional Text
  Binding: Health
```

## 7. Editor UX

### 7.1 UI Editor Panel

新規ドックパネルとして `UIEditorPanel` を追加する。

推奨メニュー:

```text
Window > UI Editor
```

パネル構成は、汎用UIエディター風の3カラムではなく、HPゲージ作成に集中した右サイドパネルにする。  
SceneViewを主役にし、UI Editor Panelは「作成」「選択中HPゲージ編集」「プレビュー」だけを持つ。

```text
+---------------------------------------------------------------+
| SceneView / UI Edit Mode                                      |
|                                                               |
|   2D Canvas, HP Gauge gizmo, safe area, anchors               |
|                                                               |
+--------------------------------------+------------------------+
| Status: Canvas / Resolution / Snap    | HP Gauge Builder      |
|                                      |                        |
|                                      | Templates              |
|                                      | [Player HP]            |
|                                      | [Enemy HP]             |
|                                      | [Boss HP]              |
|                                      |                        |
|                                      | Selected Gauge         |
|                                      | Target                 |
|                                      | Layout                 |
|                                      | Visual                 |
|                                      | Text                   |
|                                      | Preview                |
|                                      | Prefab                 |
+--------------------------------------+------------------------+
```

既存のSceneViewと完全に別画面にする必要はない。  
初期実装ではSceneViewに `2D UI Edit Mode` を追加し、右側の `HP Gauge Builder` がテンプレート作成とInspectorを兼ねる。

### 7.1.1 HP Gauge Builder Layout

右サイドパネルは以下の順に並べる。

```text
HP Gauge Builder

Canvas
  Active Canvas: BattleHUD_Canvas
  Resolution: 1920 x 1080
  [Create Canvas]

Templates
  [Player HP]
  [Enemy HP]
  [Boss HP]

Parts
  [Canvas]
  [Empty Gauge Root]
  [Image]
  [Fill Image]
  [Damage Preview]
  [HP Text]

Selected Gauge
  Name
  Target Mode
  Explicit Target

Layout
  Anchor Preset
  Position
  Size
  Pivot

Visual
  Background Texture
  Fill Texture
  Fill Direction
  Fill Color
  Damage Preview Color

Text
  Visible
  Format
  Font
  Font Size
  Color

Preview
  Current HP
  Max HP
  100% / 75% / 50% / 25% / 0%

Prefab
  [Save as HP Gauge Prefab]
  [Apply Prefab]
```

重要:

- 初期完成版では左Paletteを常設しない。
- 右サイドパネル内に `Templates` と `Parts` を分けて置く。
- `Templates` は完成済みHPゲージを一発作成する補助機能。
- `Parts` は細かい部品を手動で組むための機能。
- ユーザーの主操作は `Templates` で素早く始め、必要に応じて `Parts` とInspectorで調整すること。
- 作成後は自動で作成したHP Gauge Rootを選択し、`Selected Gauge` に編集項目を出す。
- 子要素を直接編集したい場合だけHierarchy / SceneViewから個別選択する。

### 7.2 Toolbar

ToolbarはSceneView下部または上部の薄いバーに寄せる。  
右サイドパネルと重複する項目を増やさない。

Toolbarには以下だけを置く。

- Canvas選択
- Preview解像度
- Safe Area表示
- Snap有効
- Grid有効
- Pixel Snap
- Gizmo Mode

Preview解像度プリセット:

- 1280 x 720
- 1920 x 1080
- 2560 x 1440
- 3840 x 2160
- Custom

### 7.3 Palette

Paletteは今回、右サイドパネル内の `Templates` と `Parts` として扱う。  
常設の大きな左Paletteは置かないが、細かい部品を組む入口は必ず残す。

テンプレートボタン:

- Player HP
- Enemy HP
- Boss HP

テンプレートボタンを押すと、現在選択中Canvasの子として完成済みHP Gaugeを作成する。

部品ボタン:

- Canvas
- Empty Gauge Root
- Image
- Fill Image
- Damage Preview
- HP Text

部品ボタンは、独自構成のHPゲージを手作業で組むために使う。  
テンプレートボタンはそれを置き換えるものではなく、よく使う構成を一発生成する補助機能である。

今回の完成条件は以下の両方を満たすこと。

- HPテンプレートボタンを押すだけで完成済みHPゲージが作れる。
- 部品ボタンを使って、同等のHPゲージを手動構築できる。

Button、Menu、Lock-on Reticle、Stamina Gaugeは将来カテゴリとして扱い、今回の完成条件には含めない。

### 7.4 Designer / SceneView

DesignerはSceneView上の2D Canvas編集モードとして扱う。

必要な操作:

- Widget選択
- 移動
- リサイズ
- 回転
- Pivot編集
- Anchor編集
- 親子付け
- Duplicate
- Delete
- Align
- Distribute
- Bring Forward / Send Backward
- Lock / Hide
- Snap to Pixel
- Snap to Grid

Gizmo表示:

- Rect枠
- Pivot点
- Anchor点
- 親Canvas境界
- Safe Area
- 選択中Widget名
- Fill方向プレビュー

### 7.5 Inspector

選択Widgetに応じて、以下のセクションを表示する。

- Identity
- Rect Transform
- Canvas Item
- Visual
- Layout
- Interaction
- Binding
- Preview
- Prefab

HP Gaugeテンプレートを選んだ場合も、専用画面を作りすぎない。  
RootにはBinding、FillにはImage Fill、TextにはText Bindingが表示される。

## 8. Data Model

### 8.1 UICanvasComponent

Canvas rootに付与する。

```cpp
enum class UICanvasRenderMode : uint8_t
{
    ScreenSpaceOverlay = 0,
    ScreenSpaceCamera = 1,
    WorldSpace = 2
};

enum class UICanvasScaleMode : uint8_t
{
    ConstantPixelSize = 0,
    ScaleWithScreenSize = 1,
    ConstantPhysicalSize = 2
};

struct UICanvasComponent
{
    UICanvasRenderMode renderMode = UICanvasRenderMode::ScreenSpaceOverlay;
    UICanvasScaleMode scaleMode = UICanvasScaleMode::ScaleWithScreenSize;

    DirectX::XMFLOAT2 referenceResolution = { 1920.0f, 1080.0f };
    float matchWidthOrHeight = 0.5f;
    float planeDistance = 100.0f;

    int sortingLayer = 0;
    int orderInLayer = 0;

    bool useSafeArea = false;
    bool pixelPerfect = false;
    bool visibleInGame = true;
    bool editableInSceneView = true;
};
```

初期実装は `ScreenSpaceOverlay` のみでよい。  
`ScreenSpaceCamera` と `WorldSpace` は将来拡張。

### 8.2 UIWidgetComponent

UI entityをエディター上でWidgetとして扱うためのメタ情報。

```cpp
enum class UIWidgetKind : uint8_t
{
    Empty = 0,
    Panel,
    Image,
    Text,
    Button,
    ProgressBar,
    Gauge,
    TemplateRoot
};

struct UIWidgetComponent
{
    UIWidgetKind kind = UIWidgetKind::Empty;
    std::string displayName;
    std::string templateId;

    bool locked = false;
    bool editorHidden = false;
    bool selectable = true;
};
```

このComponentはRuntime描画には直接不要。  
EditorでのPalette、Hierarchy、Inspector表示、Template識別に使う。

### 8.3 RectTransformComponent

既存Componentを継続利用する。

既存:

```cpp
struct RectTransformComponent
{
    DirectX::XMFLOAT2 anchoredPosition;
    DirectX::XMFLOAT2 sizeDelta;
    DirectX::XMFLOAT2 anchorMin;
    DirectX::XMFLOAT2 anchorMax;
    DirectX::XMFLOAT2 pivot;
    float rotationZ;
    DirectX::XMFLOAT2 scale2D;
};
```

追加検討:

```cpp
DirectX::XMFLOAT4 margin;
bool stretchWidth;
bool stretchHeight;
```

ただし初期実装では追加せず、`anchorMin != anchorMax` の場合だけInspectorでStretch表示にする。

### 8.4 CanvasItemComponent

既存Componentを継続利用する。

不足が出た場合は以下を追加する。

```cpp
float alpha = 1.0f;
bool raycastTarget = true;
```

`interactable` は入力可否、`raycastTarget` はヒットテスト対象かどうかとして分けたい。  
初期実装では `interactable` を併用してよい。

### 8.5 UIImageComponent

既存の `SpriteComponent` はTextureとTintだけを持つ。  
UIではFill、Sliced、Tiledが必要になるため、SpriteComponentを壊さず、UI専用の追加Componentを用意する。

```cpp
enum class UIImageDrawMode : uint8_t
{
    Simple = 0,
    Sliced = 1,
    Tiled = 2,
    Filled = 3
};

enum class UIImageFillMethod : uint8_t
{
    Horizontal = 0,
    Vertical = 1,
    Radial90 = 2,
    Radial180 = 3,
    Radial360 = 4
};

enum class UIImageFillOrigin : uint8_t
{
    Left = 0,
    Right = 1,
    Bottom = 2,
    Top = 3
};

struct UIImageComponent
{
    UIImageDrawMode drawMode = UIImageDrawMode::Simple;
    UIImageFillMethod fillMethod = UIImageFillMethod::Horizontal;
    UIImageFillOrigin fillOrigin = UIImageFillOrigin::Left;

    float fillAmount = 1.0f;
    DirectX::XMFLOAT4 border = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool preserveAspect = false;
};
```

描画時は `SpriteComponent` の `textureAssetPath` と `tint` を使い、`UIImageComponent` で描画方式を補う。

### 8.6 UIProgressComponent

HPゲージ、スタミナゲージ、ボスゲージなどの共通Progress表現。

```cpp
enum class UIProgressDirection : uint8_t
{
    LeftToRight = 0,
    RightToLeft = 1,
    BottomToTop = 2,
    TopToBottom = 3
};

struct UIProgressComponent
{
    EntityID fillImage = Entity::NULL_ID;
    EntityID delayedFillImage = Entity::NULL_ID;
    EntityID textEntity = Entity::NULL_ID;

    UIProgressDirection direction = UIProgressDirection::LeftToRight;

    float value = 1.0f;
    float displayedValue = 1.0f;
    float delayedValue = 1.0f;

    float smoothingSpeed = 12.0f;
    float delayedSpeed = 6.0f;
    float delayedWait = 0.35f;

    bool clamp01 = true;
};
```

`HPGaugeFillComponent` は将来的に `UIProgressComponent` + `UIValueBindingComponent` へ統合する。  
既存HPGaugeは互換のため残し、テンプレート作成時には新Componentを優先する。

### 8.7 UIValueBindingComponent

ゲーム値とUI表示を接続する。

```cpp
enum class UIValueSourceKind : uint8_t
{
    Manual = 0,
    Health = 1,
    Stamina = 2,
    LockOn = 3,
    BattleTimer = 4,
    ScriptVariable = 5
};

enum class UIBindingTargetMode : uint8_t
{
    ExplicitEntity = 0,
    FirstPlayer = 1,
    FirstEnemy = 2,
    FirstBoss = 3,
    LockedOnTarget = 4,
    EventSource = 5
};

struct UIValueBindingComponent
{
    UIValueSourceKind sourceKind = UIValueSourceKind::Manual;
    UIBindingTargetMode targetMode = UIBindingTargetMode::ExplicitEntity;
    EntityID explicitTarget = Entity::NULL_ID;

    std::string variableName;

    float current = 1.0f;
    float max = 1.0f;
    float normalized = 1.0f;

    bool targetValid = false;
    bool hideWhenNoTarget = false;
    bool hideWhenZero = false;
    bool hideWhenFull = false;
};
```

HPゲージは以下の設定になる。

```text
sourceKind = Health
targetMode = FirstPlayer / ExplicitEntity / FirstBoss
```

### 8.8 UITextBindingComponent

TextComponentの文字列をゲーム値から生成する。

```cpp
enum class UITextBindingFormat : uint8_t
{
    None = 0,
    CurrentMax = 1,
    CurrentOnly = 2,
    Percent = 3,
    LabelCurrentMax = 4,
    CustomFormat = 5
};

struct UITextBindingComponent
{
    EntityID valueBinding = Entity::NULL_ID;
    UITextBindingFormat format = UITextBindingFormat::CurrentMax;
    std::string label = "HP";
    std::string customFormat = "{current} / {max}";
};
```

### 8.9 UILayoutComponent

初期は必須ではないが、メニューやボタン列で必要になる。

```cpp
enum class UILayoutMode : uint8_t
{
    None = 0,
    Horizontal = 1,
    Vertical = 2,
    Grid = 3
};

struct UILayoutComponent
{
    UILayoutMode mode = UILayoutMode::None;
    DirectX::XMFLOAT4 padding = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT2 spacing = { 8.0f, 8.0f };
    DirectX::XMFLOAT2 cellSize = { 100.0f, 40.0f };

    bool controlChildWidth = true;
    bool controlChildHeight = true;
    bool childForceExpandWidth = false;
    bool childForceExpandHeight = false;
};
```

初期実装ではEditor表示だけにしてもよい。Runtime自動レイアウトはPhase 3以降。

## 9. Asset and Save Model

### 9.1 Scene Entity First

初期実装では、UIはScene内のentityとして保存する。  
`PrefabSystem::SaveRegistryAsScene` / `SaveEntityAsPrefab` の流れをそのまま使う。

### 9.2 Widget Prefab

再利用可能UIはPrefabとして保存する。

推奨配置:

```text
Data/UI/Prefabs/
  PlayerHP.prefab
  BossHP.prefab
  LockOnReticle.prefab
  MainMenuButton.prefab
```

UI Editorの `Save as Widget Prefab` は内部的には `PrefabSystem::SaveEntityAsPrefab` を呼ぶ。

### 9.3 UI Document

将来、複数CanvasをまとめたUIアセットが必要になった場合は `.ui` を追加する。

```text
Data/UI/Documents/BattleHUD.ui
```

ただし最初から専用ファイル形式を作らない。  
まずScene + Prefabで運用し、保存形式が苦しくなってからUI Documentを追加する。

## 10. Runtime Flow

Runtime更新順の目安:

```text
HealthSystem
UIValueBindingSystem
UIProgressSystem
UITextBindingSystem
UI2DSpriteExtractSystem
UI2DTextExtractSystem
UI2DDrawSystem / Render
```

役割:

- `UIValueBindingSystem`: Healthなどから値を読む。
- `UIProgressSystem`: normalized値をFill Imageへ反映する。
- `UITextBindingSystem`: TextComponent.textを更新する。
- `UI2DExtractSystem`: 描画キューへ流す。

GameLayerはUI生成をしない。  
GameLayerはSceneを読み込むだけに近づける。

## 11. HP Gauge as Template

HPゲージは以下のPrefab Templateとして作る。

```text
PlayerHPGauge_Template
  Root
    Components:
      UIWidgetComponent(kind = ProgressBar)
      UIValueBindingComponent(sourceKind = Health, targetMode = FirstPlayer)
      UIProgressComponent
      RectTransformComponent
      CanvasItemComponent

  Background
    Components:
      UIWidgetComponent(kind = Image)
      RectTransformComponent
      CanvasItemComponent
      SpriteComponent
      UIImageComponent(drawMode = Sliced)

  Fill
    Components:
      UIWidgetComponent(kind = Image)
      RectTransformComponent
      CanvasItemComponent
      SpriteComponent
      UIImageComponent(drawMode = Filled)

  DamagePreview
    Components:
      UIWidgetComponent(kind = Image)
      RectTransformComponent
      CanvasItemComponent
      SpriteComponent
      UIImageComponent(drawMode = Filled)

  Text
    Components:
      UIWidgetComponent(kind = Text)
      RectTransformComponent
      CanvasItemComponent
      TextComponent
      UITextBindingComponent(format = CurrentMax)
```

InspectorではRootを選択すると以下をまとめて見せる。

- Target
- Current Preview HP
- Max Preview HP
- Smoothing
- Damage Preview
- Fill Direction
- Text Format
- Colors

ただし内部データは各子entityのComponentに保持する。

## 12. Binding Preview

UI作成中は実際のPlayerがいなくても見た目を確認したい。

`UIValueBindingComponent` にはEditor Preview値を持たせる案がある。

```cpp
struct UIPreviewValueComponent
{
    bool overrideInEditor = true;
    float previewCurrent = 75.0f;
    float previewMax = 100.0f;
    float previewNormalized = 0.75f;
};
```

Preview Mode:

- 100%
- 75%
- 50%
- 25%
- 0%
- Custom
- Runtime

Runtimeでは `UIPreviewValueComponent` は無視する。

## 13. Inspector Details

### 13.1 Rect Transform Section

表示項目:

- Anchor Preset
- Anchor Min
- Anchor Max
- Anchored Position
- Size Delta
- Pivot
- Rotation Z
- Scale 2D

Anchor Preset:

- Top Left
- Top
- Top Right
- Left
- Center
- Right
- Bottom Left
- Bottom
- Bottom Right
- Stretch Horizontal
- Stretch Vertical
- Stretch Full

### 13.2 Visual Section

Image:

- Texture
- Tint
- Draw Mode
- Preserve Aspect
- Fill Amount
- Fill Direction
- Border

Text:

- Text
- Font
- Font Size
- Color
- Alignment
- Line Spacing
- Wrapping

### 13.3 Binding Section

- Source Kind
- Target Mode
- Explicit Target
- Hide Rules
- Smoothing
- Preview Values

### 13.4 Interaction Section

- Button ID
- Enabled
- Raycast Target
- Hover Tint
- Pressed Tint
- Disabled Tint
- Click Event Preview

## 14. SceneView Editing Rules

### 14.1 Selection

UI entityは `CanvasItemComponent` + `RectTransformComponent` を持つものを選択可能にする。  
クリック時は `UIHitTestSystem::PickTopmost` を使う。

### 14.2 Transform

SceneView上の操作は `RectTransformComponent` を編集する。  
`TransformComponent` のworld値を直接編集しない。

`TransformSystem` または新規 `RectTransformSystem` が、Canvasと親子関係から最終Transformを計算する。

### 14.3 Undo

以下はUndo対象。

- Widget作成
- Widget削除
- Rect変更
- 親子変更
- Component値変更
- Prefab Apply
- Prefab Unpack

既存 `UndoSystem` に乗せる。

## 15. Template Creation Commands

UI Editorに以下の作成コマンドを追加する。  
今回必須なのはHP Gauge Templateと、HPゲージを構成する部品作成である。  
テンプレートは補助機能であり、細かい部品を組む機能も完成条件に含める。

```text
Create > UI > Canvas
Create > UI > HP Gauge > Player HP
Create > UI > HP Gauge > Enemy HP
Create > UI > HP Gauge > Boss HP
Create > UI > HP Gauge Parts > Empty Gauge Root
Create > UI > HP Gauge Parts > Image
Create > UI > HP Gauge Parts > Fill Image
Create > UI > HP Gauge Parts > Damage Preview
Create > UI > HP Gauge Parts > HP Text
```

右サイドパネルには同じ処理を呼ぶテンプレートボタンを置く。

```text
HP Gauge Builder > Templates
  [Player HP]
  [Enemy HP]
  [Boss HP]

HP Gauge Builder > Parts
  [Canvas]
  [Empty Gauge Root]
  [Image]
  [Fill Image]
  [Damage Preview]
  [HP Text]
```

テンプレート作成時:

1. Canvasがなければ作成確認を出す。
2. Canvasがある場合は現在のActive Canvasへ追加する。
3. 押したテンプレート種別に応じてTarget Modeを設定する。
4. Root entityを作る。
5. Background / Fill / DamagePreview / Textを子に作る。
6. RootにBindingとProgressを付ける。
7. FillとDamagePreviewにImage Fill設定を付ける。
8. TextにText Bindingを付ける。
9. デフォルトの見た目、サイズ、位置を設定する。
10. 作成したRootを選択状態にする。
11. 右サイドパネルの `Selected Gauge` に編集項目を表示する。

テンプレートごとの初期値:

```text
Player HP
  targetMode = FirstPlayer
  anchor = Top Left
  position = (48, 48)
  size = (420, 42)
  text = CurrentMax

Enemy HP
  targetMode = FirstEnemy
  anchor = Top Right
  position = (-48, 48)
  size = (320, 32)
  text = Percent

Boss HP
  targetMode = FirstBoss
  anchor = Top Center
  position = (0, 72)
  size = (820, 36)
  text = LabelCurrentMax
```

このテンプレートボタンを押した直後に、最低限ゲームで使えるHPゲージが完成していること。  
追加調整は任意であり、必須作業にしない。

部品作成時:

1. Canvasがなければ作成確認を出す。
2. 選択中HP Gauge Rootがある場合は、その子として部品を追加する。
3. 選択中HP Gauge Rootがない場合、部品に応じてRoot作成を促す。
4. `Image` は通常の背景/装飾画像として作る。
5. `Fill Image` は `UIImageComponent(drawMode = Filled)` を持つ画像として作る。
6. `Damage Preview` は遅延追従用Fill Imageとして作る。
7. `HP Text` は `TextComponent` と `UITextBindingComponent` を持つTextとして作る。
8. 作成した部品を選択状態にする。
9. Root側の `UIProgressComponent` に未設定の参照があれば自動接続する。

部品作成の目的は、テンプレートでは足りない独自HPゲージを作れるようにすること。  
ただし、部品を作るだけで接続が壊れやすい設計にはしない。可能な限り、現在選択中Rootへ自動接続する。

## 16. Rendering Changes

必要な描画拡張:

- `UI2DSpritePacket` にFill情報を追加する。
- `UI2DSpriteRenderSystem` でFilled Imageを描画する。
- Sliced Image用のnine-slice描画を追加する。
- Canvas単位でreference resolutionを反映する。
- sortingLayer / orderInLayer / hierarchy orderのソートを安定させる。

初期優先度:

1. Filled Image
2. Canvas scale
3. Stable sorting
4. Nine-slice
5. Mask / clipping

## 17. Interaction

Interactionは将来拡張。  
今回のHPゲージ完成条件には含めない。

ボタンは既存 `UIButtonComponent` を活かす。

追加したい概念:

```cpp
enum class UIInteractionState : uint8_t
{
    Normal = 0,
    Hovered = 1,
    Pressed = 2,
    Disabled = 3
};

struct UIInteractionStateComponent
{
    UIInteractionState state = UIInteractionState::Normal;
    bool pointerInside = false;
    bool pointerDown = false;
};
```

Buttonの見た目変更は、最初はTintだけでよい。

## 18. Implementation Plan

Phase 1からPhase 3までを今回のHPゲージ完成範囲とする。  
Phase 4以降はUIエディターを汎用化するときの将来作業。

### Phase 1: Editor Foundation

- `UICanvasComponent` 追加
- `UIWidgetComponent` 追加
- Create UI Canvas command
- SceneView 2D UI Edit Mode
- UI entity選択
- RectTransform gizmo
- UI Palette最小版
- UI Inspector最小版

完了条件:

- SceneViewでCanvas上にImage/Textを置ける。
- 位置とサイズをSceneViewとInspectorで編集できる。
- Scene保存・読み込みで復元される。

### Phase 2: Progress / HP Gauge Template

- `UIImageComponent` 追加
- Filled Image描画
- `UIProgressComponent` 追加
- `UIValueBindingComponent` 追加
- `UITextBindingComponent` 追加
- HP Gauge Template作成
- Preview値編集

完了条件:

- UI EditorからPlayer HP Gaugeを作れる。
- HealthComponentに接続してRuntimeで変化する。
- GameLayer直書きHUDなしでHP表示できる。

### Phase 3: Prefab Workflow

- Save as Widget Prefab
- Instantiate Widget Prefab
- Apply / Revert / Unpack UI Prefab
- Template PaletteをData/UI/Prefabsから生成

完了条件:

- 作ったHP GaugeをPrefab化できる。
- 別Sceneに配置して再利用できる。

### Phase 4: Layout and Interaction

将来拡張。今回の完成条件には含めない。

- UILayoutComponent
- Button visual state
- Event ID一覧
- Alignment / Distribute tools
- Safe Area preview

完了条件:

- メニュー画面のボタン列をUI Editorだけで作れる。

### Phase 5: Polish

将来拡張。今回の完成条件には含めない。

- Nine-slice
- Mask / Clip Rect
- Font style preset
- Theme preset
- UI animation hooks
- Device preview presets

## 19. Migration Plan

既存HPGaugeComponentはすぐ消さない。

移行手順:

1. 既存HPGaugeをそのまま動かす。
2. 新UI Editorで同等のHP Gauge Templateを作る。
3. GameLayerの旧HUD生成を無効化できるフラグを作る。
4. Sceneに配置した新HP Gaugeで置き換える。
5. 旧 `UIHPText2D` / `UIProgressBar2D` などの直書きHUD系を段階的に整理する。

## 20. Acceptance Criteria

この章が今回の実装完了条件である。  
対象はHPゲージ作成のみ。

### Authoring

- 新規SceneでCanvasを作成できる。
- `Player HP` / `Enemy HP` / `Boss HP` のテンプレートボタンから完成済みHP Gaugeを作成できる。
- `Canvas` / `Empty Gauge Root` / `Image` / `Fill Image` / `Damage Preview` / `HP Text` の部品ボタンから手動でHP Gaugeを構築できる。
- HP Gauge作成時に `Root / Background / Fill / DamagePreview / Text` の階層が自動生成される。
- 部品作成時、選択中HP Gauge Rootへ自動で親子付け・参照接続される。
- SceneView上でHP Gauge全体を移動・リサイズできる。
- SceneViewまたはHierarchyからBackground、Fill、DamagePreview、Textを個別選択できる。
- InspectorでAnchor、Pivot、Size、Color、Font、Texture、Fill方向を編集できる。
- InspectorでTarget Mode、Explicit Target、Preview Current、Preview Maxを編集できる。
- 作成したHP GaugeがScene保存・読み込みで復元される。
- HP GaugeをPrefabとして保存できる。
- 保存したHP Gauge Prefabを別Sceneへ配置できる。

### Runtime

- Player HPが減るとHP GaugeのFillが減る。
- EnemyまたはBossをExplicit Targetとして指定できる。
- Boss HP Gaugeを別Targetとして作れる。
- DamagePreviewが遅れて追従する。
- Textが `current / max`、`current only`、`percent` のいずれかで更新される。
- Targetが存在しない場合の表示/非表示を設定できる。
- `GameLayer` に個別HPゲージ生成コードを追加しなくてよい。
- 1280x720と1920x1080でCanvas Scaleが破綻しない。

### Editor Quality

- 選択中HP Gaugeまたは子要素がSceneView上で分かる。
- HP Gaugeの子要素が重なっていても表示順に従って選択できる。
- Undo / Redoできる。
- Prefab化したHP Gaugeを再配置できる。

### Out of Scope for Completion

- Button作成
- メニュー画面作成
- Lock-on Reticle作成
- Stamina Gauge作成
- 汎用Panel作成
- UI Animation
- Layout Group
- Widget Blueprint / Visual Scripting

## 21. Risks

### 21.1 RectTransformとTransformの二重管理

UIはRectTransformを編集し、Transformは計算結果として扱うべき。  
両方を手編集できると破綻する。

対策:

- UI Edit ModeではTransform編集を隠す。
- InspectorはRectTransformを主に表示する。
- TransformSystem側でUI entityの扱いを明確化する。

### 21.2 Bindingが専用Componentだらけになる

HP、スタミナ、MP、ボスHPごとにComponentを増やすと管理不能になる。

対策:

- `UIValueBindingComponent` に統合する。
- 専用テンプレートはEditor作成コマンドとして実装する。

### 21.3 Prefab Overrideが複雑化する

UIは子entityが多く、Prefab Overrideが増えやすい。

対策:

- 初期はWidget Root単位でApply / Revertする。
- 子ごとの差分表示は後回し。

### 21.4 Draw Orderが分かりづらい

Hierarchy order、sortingLayer、orderInLayerが混ざると混乱する。

対策:

- UI EditorのLayer Panelで実際の描画順を表示する。
- 同一親内はHierarchy順を基本にする。
- sortingLayer / orderInLayerはCanvasまたは大分類に限定する。

## 22. Recommended First Milestone

最初に作るべき最小セットは、HPゲージ作成に必要なものだけに限定する。

```text
UICanvasComponent
UIWidgetComponent
UIImageComponent
UIValueBindingComponent
UIProgressComponent
UITextBindingComponent
UIEditorPanel
UI Canvas Create Command
Image/Text/HP Gauge Palette
Filled Image Rendering
```

このMilestoneで、Player / Enemy / BossのHPゲージをUI Editorから作成し、Sceneに保存できる状態を目指す。

## 23. Final Direction

HPゲージ作成機能は、単体機能として完成させるより、UI Editorのテンプレート機能として完成させる方がよい。

ただし、今回の実装完了ラインはHPゲージだけに置く。  
UI Editorの内部構造は将来の汎用化を見越すが、機能として完成させる対象はPlayer / Enemy / Boss HP Gaugeである。

最終的な理想は以下。

```text
UI EditorでWidgetを作る
Prefabとして保存する
Sceneに配置する
Bindingでゲーム値につなぐ
RuntimeはSceneのUIをそのまま動かす
```

これにより、1対1アクションゲームに必要なHUD、ボスHP、メニュー、ロックオン表示、ダメージ表示を、コード直書きではなく作成ツール側で組めるようになる。
