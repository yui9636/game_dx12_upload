# UI Editor Code Review and Improvement Spec

Created: 2026-05-10

Status: Draft

Target:

- `Source/UIEditor/UIEditorPanel.h`
- `Source/UIEditor/UIEditorPanel.cpp`
- `Source/Layer/EditorLayer*.cpp`
- `Source/Component/RectTransformComponent.h`
- `Source/Component/CanvasItemComponent.h`
- `Source/Gameplay/HPGaugeComponent.h`
- `Source/Gameplay/HPGaugeSystem.cpp`
- `Source/UI/UI2DDrawSystem.h`
- `Source/UI/UIHitTestSystem.h`

## 0. 結論

現在の UI Editor は Phase 3 までの機能名はそろっているが、実体は「専用エディター」ではなく、巨大な ImGui パネルに HP Gauge 作成ボタンと簡易矩形プレビューを押し込んだ状態である。

特に問題なのは以下の 4 点。

1. Designer View が実際の UI 表示と一致していない。
2. 親子 RectTransform を正しく評価しておらず、子要素の見た目が信用できない。
3. 画面上で直接ドラッグ、リサイズ、並べ替え、削除、複製できない。
4. Preview HP が runtime 用フィールドを component に直接書き込むため、Prefab や Scene に汚れた状態が保存される危険がある。

この仕様書の目的は、既存実装をベースに延命することではなく、UI Editor v2 として「HP ゲージを作るための最低限まともな専用エディター」に作り直す基準を定義すること。

## 1. 完成条件

今回の改善の完成条件は、汎用 UI エディターの完成ではない。

完成条件:

- UI Editor を開けば、Inspector を使わずに HP ゲージを作成できる。
- `Template...` から `Player HP` / `Boss HP` を選び、テンプレートを生成できる。
- 生成された HP ゲージの内部パーツを Designer View 上で直接移動、リサイズできる。
- 子要素の Background / Fill / DamagePreview / Text が、親の位置に追従して表示される。
- Designer View の表示順が runtime の `UI2DDrawSystem` と同じになる。
- HP プレビュー値を変えても、runtime 用の `currentHP` / `targetRatio` / `displayedRatio` / `targetValid` は保存対象として汚れない。
- 作成した HP ゲージを Prefab として保存できる。
- 作成した HP ゲージ Prefab を Level Editor から SceneView に配置できる。
- Apply / Revert / Unpack が UI Editor 内で完結する。
- SceneView の左上、上中央、右上、左端、右端、右下などへの最終配置は Level Editor の Gizmo で行える。
- Undo / Redo が主要操作で破綻しない。

非ゴール:

- 汎用 Auto Layout。
- Mask / 9-slice / Rich Text。
- World Space UI。
- UI Animation Timeline。
- Button / Menu / Lock-on UI など HP ゲージ以外の完成。
- 新しい UI Runtime Component 群の大量追加。

## 2. 現行コードの問題

### 2.1 `UIEditorPanel.cpp` が肥大化している

`Source/UIEditor/UIEditorPanel.cpp` は 1 ファイルに以下をすべて持っている。

- Template data。
- Snapshot 生成。
- Canvas 検索。
- Palette 描画。
- Designer View 描画。
- Widget Tree 描画。
- Properties 描画。
- Prefab 保存、Apply、Revert、Unpack。
- Undo 記録。
- RectTransform 同期。

これにより、UI 表示、データ生成、編集コマンド、Prefab 操作の責務が混ざっている。

修正方針:

- `UIEditorPanel` はワークスペース全体のオーケストレーションだけにする。
- 実処理は後述のファイルへ分割する。

### 2.2 Designer View が WYSIWYG ではない

対象: `UIEditorPanel.cpp:558-637`

現在の `DrawDesignerView()` は以下しかしていない。

- 1920x1080 の矩形を描く。
- Safe Area と Grid を描く。
- `CollectSubtree()` 順に UI entity の矩形を塗る。
- `InvisibleButton` で選択する。

不足:

- Sprite の texture 表示。
- Text の実表示。
- Fill の `runtimeRatio` 表示。
- 親子 Transform の合成。
- `sortingLayer` / `orderInLayer` 順の描画。
- Drag move。
- Resize handle。
- Pivot / Anchor handle。
- Pan / Zoom。
- Frame selected。
- Marquee select。
- 右クリック menu。

この状態では、Scene に出る見た目を UI Editor 内で判断できない。

修正方針:

- Designer View は runtime の UI 評価に近い結果を表示する。
- 少なくとも HP ゲージについては Background / Fill / DamagePreview / Text が実際の配置関係で見えること。
- Fill は編集用 preview ratio を使って塗り幅を変える。

### 2.3 子要素の座標が親に追従していない

対象:

- `UIEditorPanel.cpp:420-424`
- `UIEditorPanel.cpp:609-612`

現在の `ToCanvasPoint()` は、各 entity の `RectTransformComponent::anchoredPosition` を常に Canvas 中心からの絶対座標として扱っている。

```cpp
origin.x + kReferenceResolution.x * 0.5f * scale + rect.anchoredPosition.x * scale
origin.y + kReferenceResolution.y * 0.5f * scale - rect.anchoredPosition.y * scale
```

しかしテンプレート生成では、Root が `(-720, 470)`、子要素が `(0, 0)` で作られる。つまり子要素は Root のローカル座標であるべきなのに、Designer View では Canvas 中心に描かれる。

これが「見た目で編集できない」最大原因。

修正方針:

- `UIRectEvaluator` を新設し、親子階層をたどって world rect を計算する。
- Designer View、hit test、authoring gizmo は同じ評価関数を使う。
- `TransformComponent::worldPosition` に依存する場合は、事前に `HierarchySystem::Update()` 済みであることを保証する。
- 可能なら RectTransform から直接 world rect を評価し、Editor 内表示と Runtime 表示の差をなくす。

必要 API 案:

```cpp
struct UIRectWorld
{
    EntityID entity = Entity::NULL_ID;
    DirectX::XMFLOAT2 center = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 size = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    float rotationZ = 0.0f;
    DirectX::XMFLOAT2 scale = { 1.0f, 1.0f };
    int sortingLayer = 0;
    int orderInLayer = 0;
};

class UIRectEvaluator
{
public:
    static bool Evaluate(Registry& registry, EntityID entity, UIRectWorld& outRect);
    static std::vector<UIRectWorld> CollectCanvasWidgets(Registry& registry, EntityID canvas);
};
```

### 2.4 描画順が runtime と違う

Runtime の `UI2DDrawSystem::CollectDrawEntries()` は以下で sort している。

1. `sortingLayer`
2. `orderInLayer`
3. `transform.worldPosition.z`
4. `entity`

一方 `UIEditorPanel.cpp:600-626` は `CollectSubtree()` の順番でそのまま描く。

修正方針:

- Designer View は `UI2DDrawSystem` と同じ sort ルールを使う。
- Tree の親子順と Render order を混同しない。
- Properties で `CanvasItemComponent::sortingLayer` / `orderInLayer` を編集できるようにする。

### 2.5 最終配置の責務が UI Editor に混ざっている

対象:

- `UIEditorPanel.cpp:403-417`
- `UIEditorPanel.cpp:1178-1210`

現在の UI Editor は `TopLeft` / `TopCenter` / `TopRight` などの Placement Preset を持っている。
しかし、これは UI Editor の責務ではない。

正しい作業手順は以下。

1. UI Editor で HP ゲージ Prefab を作成する。
2. UI Editor で Prefab に保存する。
3. Level Editor でその Prefab を Scene / Canvas に設置する。
4. SceneView 上の Gizmo で左上、上中央、右上、左端、右端、右下などへ配置する。

UI Editor は Prefab の中身を作る場所であり、Scene 上の最終配置を決める場所ではない。

現在の preset は `halfW - 260` のような固定値を返しており、widget 幅や pivot も見ていない。加えて、配置責務そのものが UI Editor に入っているため、設計として二重に良くない。

修正方針:

- UI Editor から最終配置用の Placement Preset を削除する。
- UI Editor の Designer View は Prefab 内部レイアウト編集だけを担当する。
- HP Gauge Root は Prefab authoring 用の原点、または中立位置に置く。
- Level Editor 側に UI Prefab 配置フローを用意する。
- Level Editor の SceneView Gizmo で RectTransform を直接移動できるようにする。
- 左上、上中央、右上、左端、右端、右下などの snap / preset が必要なら Level Editor 側の配置補助として実装する。

Level Editor 側の配置計算案:

```cpp
position.x = -canvasWidth * 0.5f + safeMarginX + rect.sizeDelta.x * rect.pivot.x;
position.y =  canvasHeight * 0.5f - safeMarginY - rect.sizeDelta.y * (1.0f - rect.pivot.y);
```

この計算は UI Editor の Prefab authoring では使わない。SceneView 上で Prefab instance を配置するときの補助として使う。

### 2.6 Properties が既存 component の項目を編集できていない

対象: `UIEditorPanel.cpp:692-865`

不足している主な項目:

RectTransform:

- `anchorMin`
- `anchorMax`
- `rotationZ`
- `scale2D`

CanvasItem:

- `sortingLayer`
- `orderInLayer`
- `visible`
- `interactable`
- `pixelSnap`
- `lockAspect`

Sprite:

- `textureAssetPath`

Text:

- `text`
- `fontAssetPath`
- `alignment`
- `lineSpacing`
- `wrapping`

HPGaugeFill:

- `useDisplayedRatio`
- `useDelayedRatio`
- `hideWhenNoTarget`
- `hideWhenFull`
- `minVisibleRatio`
- `midThreshold`
- `lowThreshold`

HPGaugeText:

- `label`
- `hideWhenNoTarget`
- `hideWhenDead`
- `hideWhenFull`

修正方針:

- UI Editor 専用 Properties で上記を編集可能にする。
- Inspector に逃がさない。
- HP Gauge 作成の範囲では、Texture / Font は text input でもよい。将来は Asset Picker に置き換える。

### 2.7 Preview HP が runtime 状態を汚す

対象: `UIEditorPanel.cpp:780-801`

現在の preview button は `HPGaugeBindingComponent` の以下を直接変更し、Undo にも記録している。

- `targetValid`
- `currentHP`
- `maxHP`
- `targetRatio`
- `displayedRatio`
- `delayedRatio`

これらは runtime が更新する値であり、authoring data ではない。Prefab 保存時に混ざると、編集用 preview の値が asset に残る。

修正方針:

- `UIEditorPreviewState` を新設する。
- Preview ratio / HP / maxHP は editor state にだけ持つ。
- Designer View 描画時のみ preview state を使う。
- Scene / Prefab / Undo に preview state を保存しない。

必要構造:

```cpp
struct UIEditorHPPreviewState
{
    bool enabled = true;
    float ratio = 1.0f;
    int currentHP = 100;
    int maxHP = 100;
};
```

### 2.8 Canvas が名前固定

対象:

- `UIEditorPanel.cpp:43`
- `UIEditorPanel.cpp:1143-1162`

現在は `BattleHUD_Canvas` という名前でしか Canvas を探せない。

問題:

- 複数 Canvas が扱えない。
- 名前を変更すると UI Editor が見失う。
- 既存 Scene に Canvas がある場合でも別 Canvas を作る可能性がある。

修正方針:

- Active Canvas 選択を Toolbar に追加する。
- Canvas 候補は `RectTransformComponent + CanvasItemComponent` を持ち、親がない entity または UI root として扱える entity から列挙する。
- 名前は表示名であって識別子にしない。
- `FindOrCreateCanvas()` は Active Canvas がない場合のみ作成する。

### 2.9 Prefab 操作が HP Gauge Root に限定されすぎている

対象: `UIEditorPanel.cpp:867-1108`

現在の `SaveSelectedAsPrefab()` は `FindSelectedGaugeRoot()` が必要。HP Gauge Root 以外の Image / Text / Canvas の prefab 化ができない。

今回の完成条件は HP ゲージで十分だが、専用エディターとしては以下が必要。

- HP Gauge Root を選択していれば HP Gauge Prefab として保存。
- 子要素を選択中なら、所属する HP Gauge Root を保存対象として明示表示。
- 将来のため、内部 API は `SaveWidgetAsPrefab(root)` にする。

修正方針:

- UI 表示は `Save HP Gauge Prefab` でよい。
- 内部実装は任意 widget に対応できる名前へ変更する。
- 保存対象 root を Properties / Prefab Bar に明示する。

### 2.10 Undo 記録が細かすぎる

対象:

- `UIEditorPanel.cpp:709-713`
- `UIEditorPanel.cpp:727-738`
- `UIEditorPanel.cpp:768-777`
- `UIEditorPanel.cpp:833-840`
- `UIEditorPanel.cpp:846-851`

`DragFloat` の毎フレーム変更で `RecordComponentChange()` が呼ばれる。これにより Undo stack が大量に増え、1 回のドラッグ操作を 1 undo として戻せない。

修正方針:

- Drag 開始時に before を保存。
- Drag 終了時に 1 action として Undo へ積む。
- Inspector 側の drag session 実装が参考になる。

必要 API 案:

```cpp
template<class T>
class UIEditorComponentEditSession
{
public:
    void Begin(EntityID entity, const T& before);
    void Commit(Registry& registry, EntityID entity, const T& after, const char* label);
    void Cancel();
};
```

### 2.11 TemplateFactory がコード固定

対象: `UIEditorPanel.cpp:59-266`

現在の Player / Boss HP テンプレートは C++ に直書き。

問題:

- 見た目調整にビルドが必要。
- 色、サイズ、子要素構成、font、texture が固定。
- Template と Prefab の境界が曖昧。

修正方針:

- Phase 1 では C++ factory のままでもよいが、`HPGaugeTemplateFactory.{h,cpp}` へ分離する。
- Phase 2 で `Data/UI/Templates/PlayerHP.ui_template.json` のような外部定義に移行する。
- Template button は C++ factory または template asset を呼び出すだけにする。

### 2.12 UI Editor と Level Editor / SceneView の責務分離が弱い

現在の UI Editor は独自 preview を描くが、Level Editor 上の UI Prefab 配置、SceneView 上の UI gizmo / hit test との関係が曖昧である。

既存には以下がある。

- `UIHitTestSystem`
- `EditorLayerSceneView.cpp` の RectTransform gizmo
- `UI2DDrawSystem`

修正方針:

- UI Editor v2 は HP ゲージ Prefab の authoring と保存だけを担当する。
- Level Editor は保存済み UI Prefab の Scene / Canvas への配置を担当する。
- SceneView は配置済み UI Prefab instance の移動、回転、スケール、RectTransform 調整を担当する。
- UI Editor 内 Designer View は Prefab 内部パーツの編集用 preview であり、Scene への配置先を決めない。
- 既存の UI hit test / draw entry の考え方とは座標評価を共有する。
- 共通化すべきものは `UIRectEvaluator` と `UIEditorCommands` に出す。

## 3. 改善後のファイル構成

以下へ分割する。

```text
Source/UIEditor/UIEditorPanel.h
Source/UIEditor/UIEditorPanel.cpp
Source/UIEditor/UIEditorState.h
Source/UIEditor/UIEditorCommands.h
Source/UIEditor/UIEditorCommands.cpp
Source/UIEditor/UIRectEvaluator.h
Source/UIEditor/UIRectEvaluator.cpp
Source/UIEditor/UIEditorPalettePanel.h
Source/UIEditor/UIEditorPalettePanel.cpp
Source/UIEditor/UIEditorDesignerView.h
Source/UIEditor/UIEditorDesignerView.cpp
Source/UIEditor/UIEditorWidgetTree.h
Source/UIEditor/UIEditorWidgetTree.cpp
Source/UIEditor/UIEditorPropertiesPanel.h
Source/UIEditor/UIEditorPropertiesPanel.cpp
Source/UIEditor/UIEditorPrefabPanel.h
Source/UIEditor/UIEditorPrefabPanel.cpp
Source/UIEditor/HPGaugeTemplateFactory.h
Source/UIEditor/HPGaugeTemplateFactory.cpp
```

役割:

- `UIEditorPanel`: Window / Dock / 全体レイアウト。
- `UIEditorState`: selection、active canvas、preview、view transform、tool mode。
- `UIEditorCommands`: create、delete、duplicate、rename、reparent、record undo。
- `UIRectEvaluator`: hierarchy-aware rect 評価。
- `UIEditorPalettePanel`: Template / Parts / Saved Prefabs。
- `UIEditorDesignerView`: canvas preview、hit test、drag、resize、gizmo。
- `UIEditorWidgetTree`: tree、rename、delete、duplicate、drag reorder。
- `UIEditorPropertiesPanel`: component editing。
- `UIEditorPrefabPanel`: save、apply、revert、unpack。
- `HPGaugeTemplateFactory`: Player HP / Boss HP snapshot 生成。

## 4. State 設計

`UIEditorPanel.h` の private state は少なすぎ、かつ意味が混ざっている。

新設:

```cpp
enum class UIEditorTool
{
    Select,
    Move,
    Resize,
    Pivot,
    Anchor,
    Pan
};

struct UIEditorViewState
{
    float zoom = 1.0f;
    DirectX::XMFLOAT2 pan = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 referenceResolution = { 1920.0f, 1080.0f };
    bool showSafeArea = true;
    bool showGrid = true;
    bool snapToGrid = true;
    bool pixelSnap = true;
    float gridSize = 20.0f;
};

struct UIEditorSelectionState
{
    EntityID primary = Entity::NULL_ID;
    EntityID hpGaugeRoot = Entity::NULL_ID;
    std::vector<EntityID> selected;
};

struct UIEditorCanvasState
{
    EntityID activeCanvas = Entity::NULL_ID;
    std::vector<EntityID> canvases;
};

struct UIEditorPreviewState
{
    bool hpPreviewEnabled = true;
    float hpRatio = 1.0f;
    int hpCurrent = 100;
    int hpMax = 100;
};

struct UIEditorInteractionState
{
    UIEditorTool tool = UIEditorTool::Select;
    bool dragging = false;
    EntityID hotEntity = Entity::NULL_ID;
    DirectX::XMFLOAT2 dragStartCanvas = { 0.0f, 0.0f };
};
```

## 5. Designer View 仕様

### 5.1 表示

Designer View は以下を描く。

1. Prefab authoring canvas 背景。
2. Safe Area。
3. Grid。
4. UI widget の実表示。
5. 選択枠。
6. Resize handle。
7. Pivot marker。
8. Anchor marker。

HP Gauge の表示:

- Sprite は `SpriteComponent::tint` を使った矩形表示でよい。
- `textureAssetPath` があれば将来 texture preview を行う。
- Text は `TextComponent::text` または HP preview text を表示する。
- Fill は `HPGaugeFillComponent` の direction と preview ratio に応じて塗り幅を変える。
- DamagePreview は delayed preview ratio を使う。
- HP Gauge Root の最終 scene 位置はここでは決めない。Designer View で編集するのは Prefab 内部の相対レイアウトである。

### 5.2 入力

最低限必要な入力:

| 操作 | 挙動 |
|---|---|
| Left Click | Widget 選択 |
| Drag selected body | 移動 |
| Drag resize handle | サイズ変更 |
| Mouse wheel | Zoom |
| Middle drag / Space + drag | Pan |
| Right click | Context menu |
| F | 選択 widget へ frame |
| Esc | 選択解除 |
| Delete | 削除 |
| Ctrl + D | 複製 |
| Ctrl + Z / Ctrl + Y | Undo / Redo |
| Arrow | 1px 移動 |
| Shift + Arrow | 10px 移動 |

### 5.3 Hit Test

Hit Test は表示順の逆順で行う。

1. `UIRectEvaluator::CollectCanvasWidgets()` で world rect を得る。
2. Runtime と同じ sort を行う。
3. 後ろから調べ、一番手前の widget を選ぶ。

### 5.4 Drag / Resize の Undo

Drag 中は component を更新してよいが、Undo は drag 終了時に 1 個だけ積む。

禁止:

- DragFloat / mouse drag の毎フレーム Undo 記録。

## 6. Palette 仕様

左 Palette は以下の順にする。

1. Templates
2. Parts
3. Saved Prefabs

Templates:

- `Template...` ボタンを先頭に置く。
- 押すと選択 UI が開く。
- 選択肢は当面 `Player HP` / `Boss HP` のみ。
- 各 template は名前、用途、Level Editor 側での推奨配置メモを表示する。

Parts:

- Canvas
- Gauge Root
- Image
- Fill Image
- Damage Preview
- HP Text

Saved Prefabs:

- `Data/UI/Prefabs` を表示。
- Phase 1 は保存済み Prefab の確認と選択のみ。
- Phase 2 で検索。
- Phase 3 で Level Editor への配置導線を用意する。
- Designer View への drag and drop 配置は行わない。

## 7. Properties 仕様

Properties は選択中 entity の component に応じて section を表示する。

共通:

- Name。
- Active。
- Lock。
- Delete / Duplicate / Frame。

RectTransform:

- Position。
- Size。
- Anchor Min / Max。
- Pivot。
- Rotation Z。
- Scale 2D。
- Authoring Origin / Reset Local Layout。

CanvasItem:

- Sorting Layer。
- Order In Layer。
- Visible。
- Interactable。
- Pixel Snap。
- Lock Aspect。

Sprite:

- Texture Path。
- Tint。

Text:

- Text。
- Font Path。
- Font Size。
- Color。
- Alignment。
- Line Spacing。
- Wrapping。

HP Gauge Binding:

- Target Mode。
- Explicit Target。
- Visible When No Target。
- Hide When Dead。
- Hide When Full。
- Smoothing。
- Damage Delay。
- Damage Speed。

HP Gauge Fill:

- Binding Root。
- Direction。
- Color Mode。
- Use Displayed Ratio。
- Use Delayed Ratio。
- Hide When No Target。
- Hide When Full。
- Min Visible Ratio。
- Fixed / High / Mid / Low Color。
- Mid Threshold。
- Low Threshold。

HP Gauge Text:

- Binding Root。
- Format。
- Label。
- Hide When No Target。
- Hide When Dead。
- Hide When Full。

Preview:

- Preview ratio は component ではなく `UIEditorPreviewState` を編集する。

## 8. Widget Tree 仕様

Widget Tree は Canvas 配下を表示する。

必要機能:

- 選択。
- Inline rename。
- 右クリック context menu。
- Delete。
- Duplicate。
- Save HP Gauge Prefab。
- Move Up / Move Down。
- Reparent。
- Frame in Designer。
- Visibility toggle。
- Lock toggle。

表示:

- Canvas / Gauge / Image / Fill / Text の種類が分かる icon または短い label を付ける。
- 選択中 entity と HP Gauge Root を別の色で区別する。

## 9. Prefab 仕様

### 9.1 保存

保存ボタン:

- 表示名は `Save HP Gauge Prefab`。
- 内部関数名は `SaveWidgetAsPrefab(EntityID root)` にする。
- 子要素を選択中の場合は、所属する HP Gauge Root を保存対象として表示する。

保存先:

- `PathResolver::Resolve("Data/UI/Prefabs")`。

保存前 validation:

- Root に `HPGaugeBindingComponent` がある。
- Fill が 1 つ以上ある。
- Text は任意。
- `RectTransformComponent` / `CanvasItemComponent` / `TransformComponent` が欠けていれば補完する。

### 9.2 Level Editor での配置

Prefab 配置は UI Editor では行わない。

正しい配置フロー:

1. UI Editor で HP Gauge Prefab を保存する。
2. Level Editor に戻る。
3. Asset Browser または Level Editor 側の UI Prefab Palette から Prefab を選ぶ。
4. Scene / Canvas に instantiate する。
5. SceneView 上の Gizmo で位置を決める。
6. 必要なら SceneView 側の snap / alignment preset で左上、上中央、右上、左端、右端、右下に合わせる。

Level Editor 側の要件:

- UI Prefab を active scene に instantiate できる。
- 配置先 Canvas を選べる。
- 配置後に Prefab instance root を選択する。
- 既存の RectTransform gizmo で移動できる。
- UI 用 snap / safe area alignment は Level Editor の配置補助として実装する。

UI Editor 側の要件:

- 保存済み Prefab の path を表示する。
- Level Editor で配置するための asset として正しく保存する。
- Scene 上の配置座標を Prefab authoring data に焼き込まない。

### 9.3 Apply / Revert / Unpack

対象:

- 選択 entity が prefab root の子なら、root を解決する。
- Prefab root を Prefab Bar に表示する。

Undo:

- Apply は file 変更と component override flag を 1 action として扱う。
- Revert は subtree replace を 1 action として扱う。
- Unpack は `PrefabInstanceComponent` removal を 1 action として扱う。

## 10. 実装 Phase

### Phase A: 設計の土台を直す

目的:

- 見た目と座標の信用を取り戻す。

作業:

- `UIEditorState.h` 追加。
- `UIRectEvaluator.{h,cpp}` 追加。
- `HPGaugeTemplateFactory.{h,cpp}` 追加。
- `UIEditorPanel.cpp` から template 生成を分離。
- Designer View の描画を hierarchy-aware にする。
- Preview HP を `UIEditorPreviewState` に移す。
- `kReferenceResolution` 固定を `viewState.referenceResolution` に移す。
- `FindCanvas()` を active canvas selection に置き換える。

完了条件:

- Player HP template の子要素が Root に追従して表示される。
- Root を動かすと Fill / DamagePreview / Text も一緒に動く。
- Preview HP を押しても `HPGaugeBindingComponent` の runtime fields は変更されない。
- Build が通る。

### Phase B: 直接編集できるようにする

目的:

- UI Editor を「Prefab 内部を触れるエディター」にする。

作業:

- `UIEditorDesignerView.{h,cpp}` 追加。
- Drag move。
- Resize handle。
- Hit test。
- Pan / Zoom。
- Snap。
- Keyboard shortcuts。
- Drag 操作の Undo 集約。

完了条件:

- Designer View 上で HP ゲージ内部パーツを移動できる。
- Designer View 上で HP ゲージ内部パーツをサイズ変更できる。
- HP Gauge Root の最終 Scene 配置は UI Editor では行わない。
- Undo 1 回で 1 drag 操作が戻る。
- Keyboard shortcut が UI Editor focus 中だけ効く。

### Phase C: Tree / Properties を実用化する

目的:

- Inspector に頼らず HP Gauge を調整できるようにする。

作業:

- `UIEditorWidgetTree.{h,cpp}` 追加。
- `UIEditorPropertiesPanel.{h,cpp}` 追加。
- Rename / Delete / Duplicate。
- Tree context menu。
- Properties の不足項目追加。
- UI Editor から最終配置 preset を削除する。

完了条件:

- Tree から削除、複製、rename ができる。
- Properties で Texture / Text / Font / Order / Visible / Fill threshold / Label を編集できる。
- Root / Background / Fill / DamagePreview / HP_Text の相対レイアウトが正しく編集できる。
- 最終配置は Level Editor / SceneView Gizmo の受け入れテストに移す。

### Phase D: Prefab workflow を仕上げる

目的:

- 作った HP ゲージを asset として再利用できるようにする。

作業:

- `UIEditorPrefabPanel.{h,cpp}` 追加。
- Save HP Gauge Prefab。
- Saved Prefabs list。
- Level Editor への配置導線。
- Apply / Revert / Unpack。
- Validation。
- File path 表示。

完了条件:

- Player HP を作って保存し、Level Editor から新規配置できる。
- Boss HP を作って保存し、Level Editor から新規配置できる。
- Prefab instance の子を選択しても root を解決して Apply / Revert / Unpack できる。
- 不正な HP Gauge は保存前に warning を出す。

## 11. 受け入れテスト

### 11.1 Player HP Template

1. UI Editor を開く。
2. `Template...` を押す。
3. `Player HP` を選ぶ。
4. HP ゲージが authoring canvas の中立位置に生成される。
5. Root / Background / Fill / DamagePreview / HP_Text が Tree に表示される。
6. Designer View 上で Background / Fill / DamagePreview / HP_Text をドラッグする。
7. 子要素も追従する。
8. Preview HP を 50% にする。
9. Fill 表示が半分になる。
10. Scene / Prefab 保存データに preview runtime 値が残らない。

### 11.2 Boss HP Template

1. `Template...` を押す。
2. `Boss HP` を選ぶ。
3. authoring canvas の中立位置に横長ゲージが生成される。
4. Resize handle で横幅を変更できる。
5. 子要素が Root の内部レイアウトとして追従する。

### 11.3 Prefab

1. Player HP を保存する。
2. `Data/UI/Prefabs` に prefab ができる。
3. Level Editor に戻る。
4. Asset Browser または UI Prefab Palette から保存した prefab を Scene / Canvas に配置する。
5. 配置先は Level Editor で選んだ Canvas の子になる。
6. SceneView の Gizmo で左上、上中央、右上、左端、右端、右下などに移動できる。
7. Apply / Revert / Unpack が UI Editor 内で動く。

### 11.4 Undo

1. Drag move する。
2. Undo 1 回で元の位置へ戻る。
3. Resize する。
4. Undo 1 回で元のサイズへ戻る。
5. Rename する。
6. Undo 1 回で元の名前へ戻る。

## 12. 禁止事項

- Inspector に機能を逃がさない。
- Designer View を矩形選択だけで終わらせない。
- 親子 RectTransform を無視しない。
- Preview HP を runtime component field に書き込まない。
- Drag 中に毎フレーム Undo action を積まない。
- Canvas を名前だけで特定しない。
- UI Editor に Scene 上の最終配置責務を持たせない。
- UI Editor の Placement preset に固定幅 `260.0f` のような magic number を入れない。
- UI Prefab の最終配置座標を Prefab authoring data に焼き込まない。
- `UIEditorPanel.cpp` にさらに責務を追加しない。

## 13. 最初に直すべき順番

優先順位:

1. `UIRectEvaluator` 作成。
2. Designer View の親子座標修正。
3. Preview HP を editor state に分離。
4. `HPGaugeTemplateFactory` 分離。
5. Active Canvas 導入。
6. Drag move / Resize。
7. Properties 不足項目追加。
8. Widget Tree context menu。
9. Prefab panel 分離。
10. Level Editor 側の UI Prefab 配置導線を追加。
11. Pan / Zoom / Snap / keyboard shortcut。

この順番を崩すと、見た目が信用できないまま機能だけ増える。

## 14. 最低限の v2 完了ライン

v2 の最低ラインは以下。

- `UIEditorPanel.cpp` が 300 行以下。
- `UIRectEvaluator` が Designer View と hit test の共通基盤になる。
- Player HP / Boss HP template が Designer View 上で正しく見える。
- Designer View 上で Prefab 内部パーツの drag move と resize ができる。
- 保存済み HP Gauge Prefab を Level Editor から Scene / Canvas に配置し、SceneView Gizmo で最終位置を調整できる。
- HP preview が authoring data を汚さない。
- Save / Apply / Revert / Unpack が UI Editor 内で完結する。
- Instantiate と最終配置は Level Editor / SceneView Gizmo で行う。
- `MSBuild Game.vcxproj /p:Configuration=Debug /p:Platform=x64` が通る。

ここまで到達して初めて、今の Phase 3 を「完成」と呼んでよい。
