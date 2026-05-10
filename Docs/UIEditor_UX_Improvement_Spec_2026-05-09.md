# UI Editor UX Improvement Spec

Created: 2026-05-09

Status: Draft

Related:

- `Docs/UIEditor_Canvas_Authoring_Spec_2026-05-09.md`
- `Docs/UIEditor_Dedicated_HPGaugeEditor_Rebuild_Spec_2026-05-09.md`
- `Source/UIEditor/UIEditorPanel.h`
- `Source/UIEditor/UIEditorPanel.cpp`

## 0. この仕様書の位置付け

`UIEditor_Dedicated_HPGaugeEditor_Rebuild_Spec_2026-05-09.md` の Phase 1 (Workspace Shell) は実装済み。  
ただし出来上がったエディターは「箱だけ揃っているが、実際に触ると操作にならない」状態である。

この仕様書は、Rebuild Spec を **置き換えるものではなく、その「中身の質」だけを定義し直す追補仕様** である。  
Rebuild Spec が定義したワークスペース構造、データモデル、Phase 区分はそのまま生かす。

スコープは **既に存在する `UIEditorPanel` の操作品質改善のみ**。新機能、新Component、Runtime描画の変更は範囲外。

## 1. 現状の問題点（実測ベース）

`Source/UIEditor/UIEditorPanel.cpp`（1,212 行、単一ファイル）を読み、以下の致命的な操作上の欠陥が確認された。

### 1.1 Designer View が「絵」でしかない

[UIEditorPanel.cpp:558-637](../Source/UIEditor/UIEditorPanel.cpp:558) の `DrawDesignerView()` は以下しかしていない。

- 矩形を塗る。
- 枠線を描く。
- 上に `InvisibleButton` を置いてクリック選択だけ受ける。

つまり以下が **すべて未実装**。

- ドラッグでの移動
- ハンドルでのリサイズ
- Pivot ハンドル
- Anchor ハンドル
- ホイールズーム
- 中ボタン or Space ドラッグでのパン
- 重なった子の Alt + クリック貫通選択
- 矩形セレクト
- 選択中要素のフォーカス（F キー）
- ガイド線・距離表示
- マルチ選択

ユーザーは Designer View 上で **何も編集できない**。Properties パネルの DragFloat 経由でしか位置を変えられない。

### 1.2 子の描画位置が親に追従しない

[UIEditorPanel.cpp:609](../Source/UIEditor/UIEditorPanel.cpp:609) で `ToCanvasPoint(origin, scale, *rect)` を呼んでいるが、これは **常に Canvas 中心からの絶対座標として `rect->anchoredPosition` を扱っている**。

```cpp
ImVec2 ToCanvasPoint(const ImVec2& origin, float scale, const RectTransformComponent& rect)
{
    return ImVec2(
        origin.x + kReferenceResolution.x * 0.5f * scale + rect.anchoredPosition.x * scale,
        origin.y + kReferenceResolution.y * 0.5f * scale - rect.anchoredPosition.y * scale);
}
```

結果として、HP Gauge Root を `(-720, 470)` に置くと、`Background`、`Fill`、`DamagePreview`、`Text` (それぞれ ローカル `(0, 0)`) は **Canvas 中央** に描かれる。  
Root の枠と子の枠がバラバラに表示され、見たまま編集が成立しない。

### 1.3 描画順が Hierarchy 走査順任せ

[UIEditorPanel.cpp:600-626](../Source/UIEditor/UIEditorPanel.cpp:600) は `CollectSubtree` の順で `AddRectFilled` するだけで、`CanvasItemComponent::orderInLayer` も `sortingLayer` も無視している。  
Runtime 描画とエディタ表示で重なり順が一致しない。

### 1.4 Pivot 計算式の不整合

[UIEditorPanel.cpp:611](../Source/UIEditor/UIEditorPanel.cpp:611):

```cpp
const ImVec2 rectMin(center.x - size.x * rect->pivot.x, center.y - size.y * (1.0f - rect->pivot.y));
```

X は `pivot.x`、Y は `(1 - pivot.y)`。Y 軸反転を pivot に焼き込んでいるため、`pivot = (0.5, 1.0)` を指定すると上端ではなく下端が中心になる、といった直感に反する挙動になる。

### 1.5 Toolbar が機能していない

[UIEditorPanel.cpp:482-499](../Source/UIEditor/UIEditorPanel.cpp:482) の `DrawToolbar()`:

| 表示 | 状態 |
|---|---|
| `Snap` チェックボックス | ドラッグ自体が無いので作用先なし |
| `Pixel Snap` (m_pixelSnap) | 同上 |
| `Safe Area` | OK（Designer View 内で枠を描くだけ） |
| `Grid` | OK（120px 固定の薄い線） |
| Resolution プリセット | `1920 x 1080` という **テキスト** のみ。切替不能 |
| Gizmo Mode | 存在しない |

Rebuild Spec §5 で要求された `Preview解像度プリセット` (1280x720 / 1920x1080 / 2560x1440 / 3840x2160 / Custom) と Gizmo Mode 切替は両方とも未実装。

### 1.6 Palette の操作コストが高い

[UIEditorPanel.cpp:501-556](../Source/UIEditor/UIEditorPanel.cpp:501) の `DrawPalette()`:

- `Templates` セクションは `Template...` ボタン → Popup → MenuItem の **2クリック** が必要。常時 `Player HP` / `Boss HP` ボタンを並べるべき（Rebuild Spec §5.1 の通り）。
- アイコンなし。`ICON_FA_LAYER_GROUP` をタイトルにだけ使い、Palette 内では文字のみ。
- `Saved Prefabs` セクションは `SmallButton` のリストでサムネイルなし、検索なし、ドラッグ&ドロップ不可。
- 区切りが `ImGui::Spacing()` のみで、視覚的な分離が弱い。

### 1.7 Widget Tree が「見るだけ」

[UIEditorPanel.cpp:639-690](../Source/UIEditor/UIEditorPanel.cpp:639) の `DrawWidgetTree()` は `TreeNodeEx` をクリック選択用に描画するだけ。以下が **すべて未実装**。

- インライン Rename
- 右クリック context menu (Delete / Duplicate / Rename / Save as Prefab / Move Up / Move Down)
- ドラッグ&ドロップでの並び替え
- ドラッグ&ドロップでの親変更
- Visibility / Lock トグル
- アイコン (Image / Text / Gauge / Canvas を見分ける)
- 検索フィルタ
- キーボードフォーカス追従

Rebuild Spec §5.3 が求めた「Renameできる / 子要素の順序を変更できる / Delete / Duplicateできる」がすべて欠けている。

### 1.8 Widget Tree が Designer View の領域を圧迫

[UIEditorPanel.cpp:560](../Source/UIEditor/UIEditorPanel.cpp:560) で `treeHeight = 150.0f` 固定、Designer View はその上の残り。  
中央カラムの縦幅が小さい解像度で Designer View が **160px の最低高に貼り付き**、Canvas が極端に縦に潰れる。スプリッター (`ImGui::Splitter`) で可変にすべき。

### 1.9 Properties パネルの欠落項目

[UIEditorPanel.cpp:692-865](../Source/UIEditor/UIEditorPanel.cpp:692) の `DrawProperties()` は HP Gauge 系の編集に偏り、汎用 UI 編集に必要なフィールドが落ちている。

| Component | 編集可能な項目 | 編集できない項目 |
|---|---|---|
| `RectTransformComponent` | Position / Size / Pivot | **Anchor Min/Max**, **Rotation Z**, **Scale 2D** |
| `CanvasItemComponent` | （なし） | **sortingLayer / orderInLayer / visible / interactable / pixelSnap / lockAspect** すべて |
| `SpriteComponent` | Tint | **textureAssetPath**（Texture が変えられない） |
| `TextComponent` | fontSize / color | **text 文字列**, **fontAssetPath**, **alignment**, **lineSpacing**, **wrapping** |
| `HPGaugeFillComponent` | direction / colorMode / Fixed/High/Mid/Low | threshold 値, hideWhenNoTarget |
| `HPGaugeTextComponent` | format | **label 編集** |

Rebuild Spec §5.4 の「Text選択時 = Text Format / Label / Font / Font Size / Color / Alignment」が満たせていない。`Label` も `Font` も `Alignment` も触れない。

### 1.10 Placement Preset が「最後に押したもの」を覚える

[UIEditorPanel.h:70](../Source/UIEditor/UIEditorPanel.h:70) の `m_lastPlacementPreset` は **エディタ全体の状態**。  
Entity A を `Top Left` にしてから Entity B を選ぶと、B の Placement Combo にも `Top Left` が表示される（B が `Top Left` でなくても）。  
本来は選択中 Entity の現在の Anchor / Pivot / Position から逆算して表示すべき。

### 1.11 Prefab 操作が HP Gauge 限定

[UIEditorPanel.cpp:867-895](../Source/UIEditor/UIEditorPanel.cpp:867) の `DrawPrefabBar()` は `FindSelectedGaugeRoot()` が空でないときだけ有効。  
Image / Text 単体の Prefab 化、Canvas 自体の Prefab 化は不可能。  
`Save as HP Gauge Prefab` というボタン名も、Rebuild Spec §13 の「Save as Widget Prefab」というジェネリックな名と一致していない。

### 1.12 ショートカット未実装

`UIEditorPanel` 内に `ImGui::Shortcut` / `IsKeyPressed` の扱いが一切ない。  
以下のキー操作が **すべて効かない**。

- `Delete` で削除
- `Ctrl + D` で複製
- `Ctrl + Z` / `Ctrl + Y` で Undo / Redo
- `Ctrl + S` で Prefab 保存
- `F` で選択にフォーカス
- 矢印キーで 1px 移動 / Shift + 矢印で 10px 移動
- `Esc` で選択解除

Undo は `UndoSystem::Instance()` 経由で内部的には繋がっているが、エディタ内からショートカットで叩けない。

### 1.13 Canvas が単一ハードコード

[UIEditorPanel.cpp:43](../Source/UIEditor/UIEditorPanel.cpp:43):

```cpp
constexpr const char* kCanvasName = "BattleHUD_Canvas";
```

[UIEditorPanel.cpp:1143-1162](../Source/UIEditor/UIEditorPanel.cpp:1143) の `FindCanvas()` は名前一致でしか Canvas を探さない。  
複数 Canvas（メイン HUD + 一時メニューなど）が成立しない。  
Rebuild Spec §5 が想定する `Active Canvas` 切替も不可能。

### 1.14 ファイル分割が未着手

Rebuild Spec §7 では以下のファイル分割を要求していた。

```
Source/UIEditor/UIEditorPanel.{h,cpp}
Source/UIEditor/UIEditorState.h
Source/UIEditor/UIEditorPalettePanel.cpp
Source/UIEditor/UIEditorDesignerView.cpp
Source/UIEditor/UIEditorWidgetTree.cpp
Source/UIEditor/UIEditorPropertiesPanel.cpp
Source/UIEditor/UIEditorPrefabPanel.cpp
Source/UIEditor/HPGaugeTemplateFactory.{h,cpp}
```

現状は `UIEditorPanel.cpp` 1ファイル（無名 namespace に約 380 行のヘルパー、メソッド本体に約 800 行）に全部詰まっている。  
今後の改修で衝突しやすく、テンプレ生成ロジックと UI 描画が同居している。

## 2. ゴール

この仕様の完成条件は、`UIEditorPanel` を **「触っていて作業が進む」品質まで持ち上げる** こと。

具体的には以下が **すべて成立** すること。

1. Designer View 上で widget をドラッグして動かせる。
2. Designer View 上のハンドルでサイズを変えられる。
3. Anchor と Pivot が gizmo で見え、ドラッグで変えられる。
4. Designer View をマウスホイールでズームし、中ボタンドラッグでパンできる。
5. 子 widget が親 widget の RectTransform に追従して描画される。
6. Toolbar の Resolution プリセットを切り替えると Canvas のリファレンス解像度が更新される。
7. Snap / Pixel Snap がドラッグ・リサイズ時に効く。
8. Widget Tree でドラッグ&ドロップで親子・順序を変えられる。
9. Widget Tree の右クリックで Delete / Duplicate / Rename / Save as Prefab が出せる。
10. Properties で `Anchor Min/Max` / `Rotation` / `Scale` / `Texture` / `Font` / `Alignment` / `Text 文字列` / `Label` / `Order in Layer` / `Visible` / `Interactable` を編集できる。
11. キーボードショートカット（Delete / Ctrl+D / Ctrl+Z / Ctrl+Y / Ctrl+S / F / Esc / 矢印 / Shift+矢印）が効く。
12. Placement Preset が選択中 Entity の現状から逆算され、複数 Entity を切り替えても矛盾しない。
13. Prefab 操作が HP Gauge に限らず任意の widget に効く（ボタン名も `Save as Widget Prefab`）。
14. 複数 Canvas が同居でき、Active Canvas を Toolbar で切り替えられる。
15. ソースファイルが Rebuild Spec §7 の構成に沿って分割されている。

## 3. 非ゴール

以下は **今回の改善範囲に含めない**。

- 新規 Component (`UICanvasComponent`, `UIWidgetComponent`, `UIImageComponent`, `UIProgressComponent`, `UIValueBindingComponent`, `UITextBindingComponent`, `UILayoutComponent`) の追加  
  → これらは Rebuild Spec の Phase 4 以降の話。今回は既存 Component (`RectTransform`, `CanvasItem`, `Sprite`, `Text`, `HPGaugeBinding`, `HPGaugeFill`, `HPGaugeText`) の上だけで品質を上げる。
- Runtime 描画の改修（Filled Image、Nine-slice、Mask 等）
- Auto Layout / Layout Group
- Animation Timeline
- Button / Menu / Lock-on UI の作成体験
- Rich Text / 縦書き / 多言語の高度サポート
- World Space UI
- 入れ子 Prefab の差分 UI

## 4. 改善項目（優先度別）

優先度は **「触れない要素を触れるようにする」もの** を P1、**「触れるが破綻するもの」** を P2、**「あれば便利」** を P3 とする。

### 4.1 P1: 触れない要素を触れるようにする

| ID | 項目 | 対象ファイル |
|---|---|---|
| P1-1 | Designer View でドラッグ移動できる | `UIEditorDesignerView.cpp` (新規) |
| P1-2 | Designer View でリサイズハンドル | 同上 |
| P1-3 | Anchor / Pivot gizmo | 同上 |
| P1-4 | ホイールズーム / 中ドラッグパン | 同上 |
| P1-5 | Widget Tree の Rename / Delete / Duplicate / Save as Prefab を context menu に | `UIEditorWidgetTree.cpp` (新規) |
| P1-6 | Widget Tree のドラッグ&ドロップ（親変更・並び替え） | 同上 |
| P1-7 | Properties に Anchor Min/Max / Rotation / Scale 追加 | `UIEditorPropertiesPanel.cpp` (新規) |
| P1-8 | Properties に Texture / Font / Text 文字列 / Alignment / Label 追加 | 同上 |
| P1-9 | Properties に CanvasItem の orderInLayer / visible / interactable 追加 | 同上 |
| P1-10 | キーボードショートカット (Delete / Ctrl+D / Ctrl+Z / Ctrl+Y / F / Esc / 矢印) | `UIEditorPanel.cpp` (Workspace 全体) |
| P1-11 | Resolution プリセット切替 | `UIEditorPanel.cpp` (Toolbar) + `UICanvasState` |

### 4.2 P2: 触れるが破綻するもの

| ID | 項目 | 対象 |
|---|---|---|
| P2-1 | 子 widget を親 RectTransform に追従描画 | Designer View の座標変換 |
| P2-2 | 描画順を `orderInLayer` 順に並べる | Designer View |
| P2-3 | Pivot 計算式の見直し（Y 軸反転を pivot に焼かない） | Designer View |
| P2-4 | Snap / Pixel Snap をドラッグ・リサイズに反映 | Designer View |
| P2-5 | Placement Preset を選択 Entity から逆算 | Properties |
| P2-6 | Prefab 操作を任意 widget へ拡張 | Prefab Bar |
| P2-7 | Templates ボタンを常時表示（Popup 廃止） | Palette |

### 4.3 P3: あれば便利

| ID | 項目 |
|---|---|
| P3-1 | 複数 Canvas 対応・Active Canvas 切替 |
| P3-2 | マルチ選択・矩形セレクト |
| P3-3 | Align / Distribute ツール |
| P3-4 | Saved Prefabs の検索・ドラッグ&ドロップ |
| P3-5 | Widget Tree の Visibility / Lock アイコン |
| P3-6 | Designer View のガイド線（中央線、選択中要素から他要素への距離） |
| P3-7 | ソースファイルの分割（Rebuild Spec §7 準拠） |

## 5. データ・状態の整理

新規 Component は追加しない。エディタ側の状態を整理するだけ。

### 5.1 UIEditorState

`Source/UIEditor/UIEditorState.h` を新設し、`UIEditorPanel` の private 状態を集約する。

```cpp
struct UIEditorViewState
{
    float canvasZoom = 1.0f;
    DirectX::XMFLOAT2 canvasPan = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 referenceResolution = { 1920.0f, 1080.0f };

    bool showSafeArea = true;
    bool showGrid = true;
    bool snapEnabled = true;
    bool pixelSnap = true;
    int gridStepPx = 8;
};

enum class UIEditorTool : uint8_t
{
    Select = 0,
    Move,
    Resize,
    Rotate,
    Pivot,
    Anchor
};

struct UIEditorInteractionState
{
    UIEditorTool tool = UIEditorTool::Select;
    EntityID hovered = Entity::NULL_ID;
    EntityID activeDrag = Entity::NULL_ID;
    DirectX::XMFLOAT2 dragStartScreen = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 dragStartAnchored = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 dragStartSize = { 0.0f, 0.0f };
    int activeHandle = -1;
};

struct UIEditorCanvasContext
{
    EntityID activeCanvas = Entity::NULL_ID;
    std::vector<EntityID> canvases;
};
```

`UIEditorPanel` は上記をメンバに持ち、各サブパネルへ参照を渡す。

### 5.2 Active Canvas の選び方（複数 Canvas 対応）

- Toolbar に Combo `Active Canvas` を置く。
- 候補は `NameComponent` を持つ entity のうち、子に `RectTransformComponent` を持つもの（または将来 `UICanvasComponent` 導入時はそれ）を列挙する。
- 暫定として `kCanvasName` 一致の自動探索は残す（互換）。
- 新規 Canvas 作成時はユーザー指定名を取る簡易ダイアログを出す。

## 6. UX 細部

### 6.1 Designer View 操作仕様

| 入力 | 動作 |
|---|---|
| 左クリック | ヒットテストで一番上の widget を選択 |
| Alt + 左クリック | 重なりの下の要素を貫通選択（連続クリックでさらに下） |
| Shift + 左クリック | 選択追加（マルチ選択、P3） |
| 左ドラッグ（widget 内） | 移動（Snap 適用） |
| 左ドラッグ（widget 外） | 矩形セレクト（P3） |
| 左ドラッグ（リサイズハンドル） | 8 方向リサイズ。Shift で縦横比固定 |
| 左ドラッグ（Pivot ハンドル） | Pivot のみ移動（位置は不変） |
| 左ドラッグ（Anchor ハンドル） | Anchor min/max を移動 |
| 中クリックドラッグ / Space + 左ドラッグ | Pan |
| ホイール | Zoom（カーソル位置中心、0.1〜4.0 倍） |
| ダブルクリック | 重なりの下の要素を一段潜って選択（Alt クリックの GUI 代替） |
| F | 選択をフレームに収める |
| Home | Zoom = 1.0 / Pan = 0 にリセット |

### 6.2 ハンドル仕様

選択中 widget の bbox に以下を表示する。

```
□──■──□
│       │
■   +   ■    ■ : リサイズハンドル (8個)
│       │    + : Pivot ハンドル
□──■──□    □ : 角ハンドル
```

- 角ハンドル: 7px 角の白塗り、選択枠と同色のフチ
- 辺ハンドル: 同サイズの ◇
- Pivot: 半径 5px の青丸（中央）
- Anchor min/max: 4 角の小さな三角（親 RectTransform の対応位置）

ハンドルのヒット範囲は描画サイズより 4px 大きくとり、小さい widget でも掴めるようにする。

### 6.3 Snap

`m_snapEnabled` がオンのとき、ドラッグ移動・リサイズで以下を適用：

- グリッド吸着: `gridStepPx` の倍数（既定 8px）
- ピクセル吸着: `pixelSnap` がオンならピクセル整数値
- 兄弟エッジ吸着: 同じ親を持つ他要素の左/中央/右、上/中央/下に 4px 以内なら吸着（P3）

### 6.4 Widget Tree 操作仕様

| 入力 | 動作 |
|---|---|
| 左クリック | 選択 |
| ダブルクリック | インライン Rename |
| 右クリック | Context Menu（Rename / Delete / Duplicate / Save as Prefab / Move Up / Move Down / Frame in Designer） |
| ドラッグ | 別ノードへドロップで親変更、ノード間にドロップで並び替え |
| Delete / Backspace | 削除 |
| Ctrl + D | 複製 |
| F2 | Rename |
| 矢印 (Up/Down) | 兄弟移動 |
| 矢印 (Right) | 開く / 子へ |
| 矢印 (Left) | 閉じる / 親へ |

### 6.5 Properties 構成

選択 Entity に応じて以下のセクションを順に表示する。Header はすべて折り畳み可能で、状態は Entity 単位でなくセクション単位で記憶する。

```text
Identity
  Name
  Active (CanvasItemComponent::visible)
  Locked (UIEditor 側のロックフラグ)

Layout (RectTransformComponent)
  Anchor Preset (9 マスのグリッド + Stretch H / V / Full の 12 個)
  Anchor Min / Max
  Anchored Position
  Size Delta
  Pivot
  Rotation Z
  Scale 2D

Canvas Item (CanvasItemComponent)
  Sorting Layer
  Order In Layer
  Interactable
  Pixel Snap
  Lock Aspect

Image (SpriteComponent)
  Texture (drag-drop AssetBrowser path)
  Tint

Text (TextComponent)
  Text
  Font (drag-drop AssetBrowser path)
  Font Size
  Color
  Alignment
  Line Spacing
  Wrapping

HP Gauge Binding (HPGaugeBindingComponent)  -- Root のときだけ
  Target Mode
  Explicit Target
  Visible When No Target / Hide When Dead / Hide When Full
  Smoothing / Damage Delay / Damage Speed
  Preview HP (100/75/50/25/0%)

Fill (HPGaugeFillComponent)  -- Fill / DamagePreview のとき
  Direction
  Color Mode
  Fixed / High / Mid / Low / Threshold

Text Binding (HPGaugeTextComponent)  -- Text のとき
  Format
  Label

Prefab (PrefabInstanceComponent)
  Prefab Path
  [Save as Widget Prefab] [Apply] [Revert] [Unpack]
```

### 6.6 Toolbar 構成

```text
[ICON] UI Editor | Canvas: [Active Canvas Combo ▼] | Resolution: [1920x1080 ▼] | Snap [□] Pixel [□] Grid [□] Safe Area [□] | Tool: [Select|Move|Rotate|Scale|Pivot|Anchor] | Zoom: [100%] [Reset] [F]
```

Resolution Combo:

- 1280 x 720
- 1920 x 1080
- 2560 x 1440
- 3840 x 2160
- Custom (数値 2 個入力)

### 6.7 Palette 構成

```text
Canvas
  [+ Create Canvas]   <-- 新規ダイアログでCanvas名入力
  Active: [BattleHUD_Canvas ▼]

Templates
  [Player HP]   <-- ボタンを直接並べる
  [Boss HP]

Parts
  [Empty Gauge Root]
  [Image]
  [Fill Image]
  [Damage Preview]
  [HP Text]

Saved Prefabs
  [Search...]
  PlayerHP_Widget.prefab    [↩ Instantiate]
  BossHP_Widget.prefab      [↩ Instantiate]
  ...
```

Saved Prefabs の各行はドラッグ可能で、Designer View 上に落とすとカーソル位置に Instantiate する。

## 7. キーボードショートカット仕様

UI Editor のワークスペースが ImGui フォーカスを持つ間のみ有効。テキスト入力中は無効。

| キー | 動作 |
|---|---|
| Delete / Backspace | 選択削除 |
| Ctrl + D | 選択複製（同じ親に） |
| Ctrl + Z | Undo |
| Ctrl + Y / Ctrl + Shift + Z | Redo |
| Ctrl + S | 選択 widget を Prefab 保存 |
| Ctrl + Shift + S | 選択 widget を Prefab 名指定で保存 |
| F | 選択を Designer View にフレーム |
| Home | Zoom = 1.0 / Pan = 0 |
| Esc | 選択解除 |
| 矢印 | 選択を 1px 移動 |
| Shift + 矢印 | 選択を 10px 移動 |
| Ctrl + 矢印 | 選択を 1 グリッドステップ移動 |
| F2 | Rename |
| Q / W / E / R / T / Y | Tool: Select / Move / Rotate / Scale / Pivot / Anchor |

実装は `UIEditorPanel::HandleShortcuts()` を新設し、`DrawWorkspace` 冒頭で呼ぶ。`ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, 0, ImGuiInputFlags_RouteFocused)` を使う。

## 8. ファイル分割（Rebuild Spec §7 準拠）

P3 だが、コード量を考えると **改修着手の早い段階で実施しないと衝突が増える**。Phase 1 の前提として進める。

```
Source/UIEditor/UIEditorPanel.{h,cpp}              -- DrawWorkspace のオーケストレーション、HandleShortcuts
Source/UIEditor/UIEditorState.h                     -- UIEditorViewState / InteractionState / CanvasContext
Source/UIEditor/UIEditorPalettePanel.{h,cpp}        -- DrawPalette
Source/UIEditor/UIEditorDesignerView.{h,cpp}        -- DrawDesignerView + ハンドル / ヒットテスト
Source/UIEditor/UIEditorWidgetTree.{h,cpp}          -- DrawWidgetTree + Context Menu / DnD
Source/UIEditor/UIEditorPropertiesPanel.{h,cpp}     -- DrawProperties (セクション分割)
Source/UIEditor/UIEditorPrefabPanel.{h,cpp}         -- DrawPrefabBar / Save / Apply / Revert / Unpack
Source/UIEditor/HPGaugeTemplateFactory.{h,cpp}      -- BuildCanvasSnapshot / BuildTemplateSnapshot / BuildPartSnapshot
Source/UIEditor/UIEditorCommands.{h,cpp}            -- 共有のヘルパー (RecordComponentChange, ExecuteCreateSnapshot, FindCanvas, ResolveGaugeRoot, etc.)
```

`UIEditorPanel` は各サブパネルに `Registry*`、`UIEditorViewState&`、`UIEditorInteractionState&`、`UIEditorCanvasContext&` を渡す。

## 9. 実装フェーズ

Rebuild Spec の Phase 1〜3 はそのまま生きる。本仕様の改修はその外側に **品質改善 Phase** として乗せる。

### Phase QI-0: ファイル分割

- §8 のファイル分割を実施。動作変更なし。
- 既存の挙動が壊れないことを確認したうえでマージ。

### Phase QI-1: Designer View Interactivity

- 4.1 P1-1〜P1-4 を実装。
- 4.2 P2-1（親追従描画）、P2-2（順序）、P2-3（Pivot）も同じ Phase で潰す。Designer View のコードを触っているうちに直さないと意味がない。
- ハンドル、ヒットテスト、ドラッグ移動・リサイズ、Snap、ズーム、パン。

完了条件:

- HP Gauge Template を作成 → Designer View 上で Root をドラッグして移動できる。
- 子要素が Root に追従して見える。
- リサイズハンドルでサイズが変わる。
- Snap / Pixel Snap が効く。

### Phase QI-2: Properties 充実 + Tree 操作

- 4.1 P1-5〜P1-9 を実装。
- 4.2 P2-5（Placement Preset 逆算）も同じ Phase で。
- Widget Tree の Rename / Delete / Duplicate / DnD / Context Menu。
- Properties に欠落フィールドを追加。

完了条件:

- Tree の右クリックから Save as Widget Prefab まで通る。
- Image の Texture を AssetBrowser から差し替えられる。
- Text の文字列を直接編集できる。
- Anchor を変えると Designer View の表示も変わる。

### Phase QI-3: Toolbar / Shortcuts / Multi-Canvas

- 4.1 P1-10, P1-11 を実装。
- 4.2 P2-6, P2-7、4.3 P3-1 を実装。
- Resolution プリセット、Active Canvas Combo、Templates 直配置、Prefab 操作汎用化、ショートカット全部。

完了条件:

- Resolution を切り替えると Designer View のリファレンス枠が変わる。
- 任意 widget で Save as Widget Prefab が押せる。
- Ctrl+Z / Ctrl+D / Delete / F が効く。
- 複数 Canvas を切り替えられる。

### Phase QI-4: Polish (P3)

- マルチ選択、矩形セレクト、Align / Distribute、Visibility / Lock、ガイド線、Saved Prefabs 検索・DnD。
- 必要になった時点でやる。完成条件には含めない。

## 10. Acceptance Criteria

UI Editor を開いてから以下を **すべて満たす** こと。

### 10.1 Designer View

- 何も選んでいない状態で widget をクリックすると選択できる。
- 選択中 widget の枠にハンドルが 8 個 + Pivot + Anchor 4 点が出る。
- 角・辺ハンドルをドラッグするとリサイズできる。
- widget 内をドラッグすると移動できる。
- Snap がオンなら 8px 単位、Pixel Snap がオンならピクセル単位に吸着する。
- ホイールで拡縮、中ドラッグでパン、F でフレーム、Home でリセットできる。
- 子 widget は親 widget の RectTransform に追従して描画される。
- 重なりは `CanvasItem.orderInLayer` 昇順で描画される。

### 10.2 Widget Tree

- ノードのダブルクリックで Rename できる。
- 右クリックで Rename / Delete / Duplicate / Save as Widget Prefab / Move Up / Move Down / Frame が出る。
- ノードをドラッグして別ノードに落とすと親が変わる。
- ノード間に落とすと兄弟順が変わる。
- Delete キーで削除、Ctrl+D で複製できる。

### 10.3 Properties

- Anchor Preset の 12 マスから選べる。
- Anchor Min / Max、Rotation Z、Scale 2D が編集できる。
- Texture と Font を AssetBrowser からドラッグして差し替えられる。
- Text の文字列、Alignment、Line Spacing、Wrapping を編集できる。
- CanvasItem の Order In Layer、Visible、Interactable を編集できる。
- HPGaugeText の Label を編集できる。
- 編集はすべて Undo / Redo 可能。
- 別 Entity を選んでも Placement Preset の表示が現状と一致する。

### 10.4 Toolbar / Shortcuts

- Resolution プリセットを切り替えると Designer View のリファレンス矩形が変わる。
- Active Canvas Combo で複数 Canvas を切り替えられる。
- Tool ボタン（Select/Move/Rotate/Scale/Pivot/Anchor）が動く。
- Delete / Ctrl+D / Ctrl+Z / Ctrl+Y / F / Home / Esc / 矢印 / Shift+矢印 がすべて効く。

### 10.5 Prefab

- HP Gauge Root 以外の任意 widget で `Save as Widget Prefab` が押せる。
- Saved Prefabs の項目を Designer View にドラッグして配置できる。
- Apply / Revert / Unpack が任意 Prefab Instance に対して動く。

### 10.6 Code

- `Source/UIEditor/` 以下が §8 のファイル構成に分割されている。
- 各サブパネルは `UIEditorViewState` / `UIEditorInteractionState` / `UIEditorCanvasContext` を介して状態をやりとりする。
- `UIEditorPanel.cpp` の行数が 300 行以下。

## 11. リスク

### 11.1 ヒットテストと描画順の不整合

`orderInLayer` 順に描画しても、ヒットテストが逆順（最前面から）でないと「見えている要素を選べない」事故になる。

対策:

- ヒットテストは描画リストを逆順に走査する。
- `Alt + クリック` で次の要素へ送る実装を最初から入れる。

### 11.2 RectTransform の親追従計算

`UIHitTestSystem` と Designer View の座標計算がズレると、見えている位置とクリックできる位置が違う問題が再発する。

対策:

- Designer View 内に `Editor2D::ComputeWorldRect(EntityID)` のようなヘルパを切り出し、ヒットテストと描画の双方が同じ関数を呼ぶ。
- Runtime 側 `UIHitTestSystem` の式を真似る形で揃える。

### 11.3 ハンドル操作の Undo 粒度

ドラッグ中にフレームごとに `RecordComponentChange` すると Undo Stack が肥大化する。

対策:

- ドラッグ開始時の `before` を保持。
- ドラッグ確定時（マウス Up）に 1 回だけ `RecordAction(ComponentUndoAction(before, after))` を発行。
- ドラッグ中の値変更は `MarkPrefabOverride` も走らせない。

### 11.4 ファイル分割のリグレッション

無名 namespace のヘルパを共有ヘッダに移すとリンクエラー / 二重定義が起きやすい。

対策:

- 共有ヘルパは `Source/UIEditor/UIEditorCommands.{h,cpp}` に集約し、内部リンケージ (`namespace { ... }`) ではなく明示の名前空間 `UIEditorInternal` を使う。

### 11.5 ショートカットの他パネルとの衝突

`Ctrl+Z` などは Scene Editor も握っており、UI Editor が前面でも誤発火する可能性がある。

対策:

- `ImGui::Shortcut(..., ImGuiInputFlags_RouteFocused)` を使い、ワークスペースが「現在アクティブな WorkspaceTab」のときだけ反応させる。

## 12. 完了の判定

§10 のチェックリストを上から順に手動で確認する。  
ひとつでも × があれば未完成。  
特に **§10.1（Designer View でドラッグ・リサイズが効く）** が満たされない時点で、何があってもこの仕様は未達である。  
理由は `「触れない要素を触れるようにする」が本仕様の根本目的だから`。
