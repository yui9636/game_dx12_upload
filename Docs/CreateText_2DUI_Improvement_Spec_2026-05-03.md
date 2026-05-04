# Create Text / 2D Text Editor Improvement Spec

作成日: 2026-05-03
対象: `Create Text`, `TextComponent`, 2D Scene View / Game View preview, Inspector editing, Font asset assignment

---

## 1. 背景

現在の `Create Text` は、2D entity を生成する入口はあるが、実用的な編集体験がほぼない。

現状確認:

- `TextComponent` は `text`, `fontAssetPath`, `fontSize`, `color`, `alignment`, `lineSpacing`, `wrapping` を持つ。
- `HierarchyECSUI.cpp` / `EditorLayerInternal.h` には `BuildSingleTextSnapshot()` と `BuildDefaultTextSnapshot()` がある。
- Scene View 側の 2D overlay は `TextComponent` を描画している。
- Font preview は `FontManager::GetEditorPreviewFont()` / `QueueEditorPreviewFont()` により editor preview 用に読み込める。
- Inspector では `TextComponent` が汎用 component meta UI で表示されるだけで、Text 専用の編集導線がない。

そのため、ユーザーから見ると `Create Text` は「作れるが、何をどう編集すればよいか分からない」「Fontや文字列の設定が自然にできない」状態になっている。

---

## 2. 最優先ゴール

1. `Create Text` 実行直後に、Scene View / Game View で読める Text が表示される。
2. 作成直後の Text entity が選択され、Inspector でそのまま編集できる。
3. Text の文字列、Font、size、color、alignment、wrapping、line spacing を Text 専用 Inspector で編集できる。
4. Font の割り当ては AssetBrowser から Inspector の Font slot へドラッグ&ドロップする操作を主導線にする。
5. Text の編集に専用の別ウィンドウ、独立 picker window、余計なポップアップ一覧を作らない。
6. Transform / RectTransform / CanvasItem の同期を保ち、作成直後から移動、回転、拡縮ができる。

---

## 3. 非ゴール

今回の Phase では以下を必須にしない。

- DX12 専用の新規 text renderer 実装。
- Rich text / inline style / emoji / ruby / vertical writing。
- Runtime input field としての完全な編集機能。
- TextMesh 的な 3D text。
- Font atlas 管理の全面刷新。
- AssetBrowser 全体の UI 再設計。
- Effect Editor、Material Editor、他ツールの texture / asset picker 改修。

---

## 4. UX 原則

### 4.1 Text は Inspector 内で完結する

Text entity を選択したら、Inspector の `TextComponent` セクションで編集する。

禁止:

- Text 編集用の個別ウィンドウを開く。
- Font 選択用の別窓を出す。
- Font一覧の大型 popup picker を TextComponent 専用に作る。
- Assetをクリックしただけで Text entity の選択を奪う。

許可:

- Inspector内の通常コントロール。
- AssetBrowser からの drag & drop。
- 既存 Material / Effect Editor 側の picker はそのまま維持する。

### 4.2 Font設定はAssetBrowserからのドラッグ固定

SpriteComponent の Texture 設定と同じ思想で、TextComponent の Font 設定は AssetBrowser からの D&D を基本にする。

TextComponent の Font slot:

- 現在の font path を表示する。
- `.ttf`, `.otf`, `.fnt` の drag & drop を受け付ける。
- Dropされた font asset を `TextComponent::fontAssetPath` に設定する。
- Font asset 以外は受け付けない。
- Drop成功時、Inspector は Text entity のまま維持する。

---

## 5. Create Text 仕様

### 5.1 Hierarchy: Create Text

Hierarchy の空白右クリック `Create Text` は、以下の component を持つ entity を作る。

- `NameComponent`
- `TransformComponent`
- `HierarchyComponent`
- `RectTransformComponent`
- `CanvasItemComponent`
- `TextComponent`

初期値:

- `NameComponent::name`: `"Text"`
- `TextComponent::text`: `"Text"`
- `TextComponent::fontAssetPath`: 既定 font path。候補は `Data/Font/ArialUni.ttf`
- `TextComponent::fontSize`: `32.0f`
- `TextComponent::color`: white
- `TextComponent::alignment`: `Center`
- `TextComponent::wrapping`: `false`
- `TextComponent::lineSpacing`: `1.0f`
- `RectTransformComponent::sizeDelta`: `320 x 80`
- `CanvasItemComponent::visible`: `true`
- `CanvasItemComponent::interactable`: `true`

作成後:

- `FinalizeCreatedEntity()` 相当の2D初期化を通す。
- 作成 entity を選択する。
- Inspector は Entity Inspector のまま。
- Scene View gizmo は作成 entity に出る。

### 5.2 Hierarchy: Create Text Child

Entity 右クリック `Create Text Child` は、選択 entity の子として同じ Text entity を作る。

Prefab lock:

- `PrefabSystem::CanCreateChild()` が false の場合は作らない。
- 既存と同じ warning を出す。

### 5.3 Font asset からの Text 作成

AssetBrowser の font asset を Hierarchy / Scene View にドラッグした場合:

- `.ttf`, `.otf`, `.fnt` を Text entity として生成する。
- `TextComponent::fontAssetPath` に drop された font path を入れる。
- Entity name と初期 text は font asset の stem を使ってよい。
- 作成直後に選択する。
- Text専用の別ウィンドウは出さない。

---

## 6. TextComponent Inspector 仕様

TextComponent は汎用 meta UI ではなく、専用 Inspector を持つ。

### 6.1 表示項目

TextComponent セクション:

- Text body
- Font slot
- Font status
- Font size
- Color
- Alignment
- Wrapping
- Line spacing
- Reset size to text bounds
- Missing component repair

### 6.2 Text body

UI:

- `InputTextMultiline`
- 幅は Inspector content 幅に追従。
- 高さは最低 72px、内容が増えた場合は 160px 程度まで伸ばす。

Undo:

- 文字入力中に毎フレーム Undo を積まない。
- 編集開始時の `TextComponent` を保存し、編集完了時に1件だけ Undo を積む。
- Enter / IME / paste を含む。

### 6.3 Font slot

Font slot は AssetBrowser D&D 専用。

表示:

- Font未設定時: `"Drop font asset here"`
- Font設定済み: file name と path
- Load失敗時: warning text

操作:

- Drag target として `ENGINE_ASSET` payload を受ける。
- 拡張子が `.ttf`, `.otf`, `.fnt` の場合だけ受ける。
- Drop成功時、`TextComponent::fontAssetPath` を変更する。
- Undo は1件。
- Asset selection は変更しない。

禁止:

- Font picker popup。
- Font一覧 window。
- AssetBrowser の font click による Text entity 選択解除。

### 6.4 Font size

UI:

- `DragFloat` または `InputFloat`
- 範囲: `1.0f` から `512.0f`
- 通常 step: `1.0f`

Undo:

- Drag中は毎フレーム Undo を積まない。
- Drag完了時に1件。

### 6.5 Color

UI:

- `ColorEdit4`

Undo:

- Color drag中に毎フレーム Undo を積まない。
- `IsItemActivated()` から `IsItemDeactivatedAfterEdit()` までを1 transaction にする。

### 6.6 Alignment

UI:

- segmented control または combo。
- 値: Left / Center / Right。

Scene View / Game View 表示:

- Left: Rect左端から表示。
- Center: Rect中央基準。
- Right: Rect右端基準。

### 6.7 Wrapping

UI:

- checkbox `Wrapping`

挙動:

- `wrapping == true` の場合、`RectTransformComponent::sizeDelta.x` を wrap width として使う。
- `wrapping == false` の場合、wrap width は 0。

### 6.8 Line spacing

UI:

- `DragFloat`
- 推奨範囲: `0.5f` から `3.0f`
- 初期値: `1.0f`

描画:

- 既存 ImGui text draw が line spacing を直接使えない場合、Phase 1 では Inspector 値の保存までを必須にし、描画反映は Phase 2 に分けてもよい。
- ただし仕様上は Game View / Scene View に反映するのが最終目標。

### 6.9 Reset size to text bounds

UI:

- button `Use Text Size`

挙動:

- 現在 font / fontSize / text / wrapping から text bounds を測る。
- `RectTransformComponent::sizeDelta` に反映する。
- RectTransform と Transform を同期する。
- Undo は RectTransform変更1件。

---

## 7. Missing Component Repair

TextComponent があるのに表示に必要な component が欠けている場合、Inspectorに修復ボタンを表示する。

必要 component:

- `TransformComponent`
- `HierarchyComponent`
- `RectTransformComponent`
- `CanvasItemComponent`

Repair:

- `Add Transform`
- `Add Hierarchy`
- `Add RectTransform`
- `Add CanvasItem`
- `Show CanvasItem`
- `Activate Entity`

注意:

- component追加後は archetype が移動するため、その frame では古い pointer を使わない。
- Repair後は次 frame で再取得する。

---

## 8. Scene View / Game View 表示仕様

### 8.1 Scene View

Scene View 2D overlay は `TextComponent` を描画する。

必須:

- Textが空でない場合、Rect内に表示される。
- Fontが未ロードの場合、default ImGui fontで表示しつつ font load を queue する。
- `fontSize`, `color`, `alignment`, `wrapping` を反映する。
- 選択 outline は Text entity でも表示する。
- Text entity の hit test は RectTransform に基づく。

### 8.2 Game View

Game View 2D overlay でも同じ Text を表示する。

必須:

- active Camera2D がある場合は Game View camera 基準で表示する。
- fallback preview 使用時は Scene View camera 基準で表示する。
- `No active 2D camera` の状態でも editor が crash/assert しない。

### 8.3 Runtime renderer

Phase 1では editor overlay 表示を優先する。

Runtime / DX12専用 renderer が未完成の場合:

- 仕様書に未対応として明記する。
- ただし Editor上の作成、編集、preview は完成条件に含める。

---

## 9. Transform / RectTransform 同期

2D Textは Sprite と同じく `RectTransformComponent` を主データとして扱う。

Scene View gizmo:

- Move: `RectTransformComponent::anchoredPosition`
- Rotate: `RectTransformComponent::rotationZ`
- Scale: `RectTransformComponent::scale2D`

Inspectorの TransformComponent 数値編集:

- Transformを編集した場合、Text entity が RectTransformを持つなら RectTransformへ同期する。
- 同期後に `HierarchySystem::MarkDirtyRecursive()` と update を行う。
- Undo は Transform と RectTransform を矛盾させない。

禁止:

- RectTransformとTransformが別々の値を持ったままになること。
- 作成直後の Text が gizmo 操作できないこと。

---

## 10. AssetBrowser / Selection 仕様

Font asset をクリックしただけでは、選択中の Text entity を奪わない。

推奨:

- AssetBrowser 単クリックは `EditorAssetContext` に active asset を入れる。
- Entity 選択中は `EditorSelection::SelectAsset()` に切り替えない。
- 明示的な `Inspect Asset` 操作だけ Asset Inspector に切り替える。

Font割り当て:

- Text Inspector の Font slot への drag & drop のみを主要導線にする。
- 右クリック `Assign to Selected Text` を追加するかは Phase 2 以降。Phase 1では不要。

---

## 11. Undo / Prefab Override

Undo単位:

- Create Text: 1 undo
- Text body edit: 1 undo per committed edit
- Font drop assign: 1 undo
- Font clear: 1 undo
- Font size drag: 1 undo per drag
- Color drag: 1 undo per drag
- Alignment change: 1 undo
- Wrapping toggle: 1 undo
- Line spacing drag: 1 undo per drag
- Use Text Size: 1 undo
- Scene View gizmo drag: 1 undo per drag

Prefab:

- TextComponent変更時は `PrefabSystem::MarkPrefabOverride()` を呼ぶ。
- RectTransform / CanvasItem / Transform変更も既存ルールに従う。

---

## 12. 実装対象ファイル候補

想定変更:

- `Source/Inspector/InspectorECSUI.cpp`
- `Source/Component/TextComponent.h`
- `Source/Hierarchy/HierarchyECSUI.cpp`
- `Source/Layer/EditorLayerInternal.h`
- `Source/Layer/EditorLayerSceneView.cpp`
- `Source/Engine/Editor2DEntityUtils.h`
- `Source/Font/FontManager.h`
- `Source/Font/FontManager.cpp`
- `Source/Generated/ComponentMeta.generated.h`

注意:

- `Game.vcxproj` / `.filters` は新規 cpp/h を追加する場合のみ変更する。
- header-only helperで済む場合は project file を触らない。
- Effect Editor / Material Editor の既存UIには影響させない。

---

## 13. 実装フェーズ

### Phase 1: Text専用Inspector

目的:

- `TextComponent` を汎用 meta UI ではなく専用UIで編集できるようにする。

作業:

- `DrawTextComponentInspector()` を追加する。
- `DrawComponentIfPresent<TextComponent>()` の代わりに専用 inspector を呼ぶ。
- Text body, font slot, font size, color, alignment, wrapping, line spacing を表示する。
- Font slot は AssetBrowser D&D のみ。
- Undo粒度を整える。

受け入れ条件:

- Text entity選択時に専用 TextComponent Inspector が出る。
- Text文字列を編集できる。
- Font assetをAssetBrowserからDropして割り当てられる。
- Font popup / 別windowは出ない。
- Effect Editor / Material Editor のUIは変わらない。

### Phase 2: Create Text初期値と作成直後の操作性

目的:

- `Create Text` 後すぐに表示・編集・移動できるようにする。

作業:

- default alignment を Center にする。
- default rect size を 320 x 80 にする。
- default font path を検証する。
- 作成直後に `FinalizeCreatedEntity()` を通す。
- 作成 entity を選択する。

受け入れ条件:

- Hierarchyの `Create Text` 直後に Scene Viewで "Text" が見える。
- InspectorはText entityを表示する。
- Scene View gizmoで移動できる。
- RectTransformとTransformが同期する。

### Phase 3: Font asset D&D生成

目的:

- Font assetをScene View / HierarchyへdragしてTextを作れるようにする。

作業:

- `.ttf`, `.otf`, `.fnt` のD&Dで Text entity を生成する。
- font path を TextComponent に入れる。
- drop位置を RectTransform anchoredPosition に反映する。

受け入れ条件:

- Font assetをScene View 2DへdropするとText entityができる。
- 作成entityにfont pathが入っている。
- 作成直後に選択され、Inspectorで編集できる。

### Phase 4: Text bounds / Use Text Size

目的:

- TextサイズにRectを合わせられるようにする。

作業:

- `FontManager` または ImGui font を使い text bounds を取得する。
- `Use Text Size` buttonを追加する。
- Undoを1件にする。

受け入れ条件:

- `Use Text Size` で RectTransform sizeDelta が文字サイズに合う。
- Undoで元の sizeDelta に戻る。

---

## 14. 受け入れ条件

### AC-1: Create Textが表示される

手順:

1. Hierarchy空白を右クリックする。
2. `Create Text` を選ぶ。

期待:

- Text entityが作成される。
- Scene View 2Dで "Text" が表示される。
- 作成entityが選択される。
- Inspectorに TextComponent 専用UIが表示される。

### AC-2: Inspectorで文字列編集できる

手順:

1. Text entityを選択する。
2. Text bodyを `"Start"` に変更する。

期待:

- Scene View / Game View の表示が `"Start"` になる。
- Undo 1回で変更前に戻る。

### AC-3: FontはAssetBrowser D&Dで設定する

手順:

1. Text entityを選択する。
2. AssetBrowserから `.ttf` を Text Inspector の Font slot へdropする。

期待:

- `TextComponent::fontAssetPath` が更新される。
- InspectorはText entityのまま。
- Font picker window / popup は出ない。
- Undo 1回で前のfont pathに戻る。

### AC-4: TextのTransform操作ができる

手順:

1. Text entityを選択する。
2. Scene View gizmoで移動する。
3. InspectorのTransform数値も変更する。

期待:

- gizmo操作はRectTransformに反映される。
- Transform数値編集もRectTransformに同期される。
- Scene View表示位置が更新される。

### AC-5: Effect Editorに影響しない

手順:

1. Effect Editorを開く。
2. 既存のEffect編集UIを操作する。

期待:

- Effect Editorのwindow構成、picker、previewは変化しない。
- TextComponent専用Inspectorの変更がEffect Editorに出ない。

### AC-6: Game Viewでassertしない

手順:

1. Game Viewを開く。
2. 2D Textを表示する。
3. Camera2Dあり / なしを切り替える。

期待:

- `ImGui::End()` assertが出ない。
- Text previewが表示される、または安全なstatus表示になる。

---

## 15. 完了定義

以下を満たしたら完了:

- AC-1 から AC-6 を確認済み。
- Debug x64 build が 0 error。
- TextComponentの編集はInspector内で完結している。
- Font設定はAssetBrowser D&D固定。
- Text専用の別window / popup pickerを追加していない。
- Effect Editor / Material Editor / SpriteComponent の既存フローを壊していない。
- 既存の未関係dirty fileをcommitしない。
