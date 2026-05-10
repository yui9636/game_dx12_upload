# UI Editor Operability Improvement Spec

Created: 2026-05-10

Status: Implementation Ready

Owner: Editor / UI Authoring

Primary Proposition: Intuitive Operation.

Primary Goal: A user must be able to create, edit, save, and place an HP Gauge through direct visual operations, without guessing where to click or relying on Inspector escape hatches.

Related:

- `Docs/UIEditor_CodeReview_Improvement_Spec_2026-05-10.md`
- `Source/UIEditor/UIEditorPanel.cpp`
- `Source/UIEditor/UIEditorState.h`
- `Source/UIEditor/UIRectEvaluator.cpp`
- `Source/UIEditor/HPGaugeTemplateFactory.cpp`
- `Source/Layer/EditorLayerSceneView.cpp`
- `Source/Asset/AssetBrowser.cpp`

## 0. Central Proposition

この仕様書の命題は **直観操作** である。

直観操作とは、ユーザーが画面上で見えている対象を、その見た目通りに選び、掴み、動かし、調整し、保存し、Level Editor に配置できることを指す。

この命題に照らすと、UI Editor / Level Editor の操作は次を満たさなければならない。

- 見えているものは選べる。
- 選べるものは hover / outline / label で分かる。
- 掴める場所は handle / cursor / highlight で分かる。
- 重なっているものは候補から選び分けられる。
- 動かした結果はその場で見える。
- Preview は asset を汚さず、見た目だけを変える。
- 保存した後、次にどこへ行けばよいか分かる。
- Level Editor では SceneView 上の Gizmo で最終配置できる。

この仕様では、機能の有無よりも「ユーザーが迷わず触れるか」を優先する。

## 1. Summary

前回の改善で、責務は以下に分離された。

- UI Editor: HP ゲージ Prefab を作る、内部レイアウトを編集する、Prefab として保存する。
- Level Editor / SceneView: 保存済み Prefab を Scene / Canvas に設置し、Gizmo で最終配置する。

しかし現状は、まだ操作体験が弱い。

- どこを掴めるのか分かりにくい。
- どの要素を選んでいるのか分かりにくい。
- 重なった UI 要素を選び分けにくい。
- Pan / Zoom / Frame が不足している。
- Widget Tree が整理・編集に使えない。
- Properties が長く、何を編集すればよいか見えにくい。
- Prefab 保存後、Level Editor へ移る導線が弱い。
- SceneView 側で UI Prefab の配置先 Canvas や alignment が弱い。

この仕様書は、次の実装で「機能がある」状態から「触って作業が進む」状態へ上げるための完成仕様である。

## 2. Final Workflow

完成後の操作フローは以下。

1. UI Editor を開く。
2. `Template...` を押す。
3. Template Picker から `Player HP` または `Boss HP` を選ぶ。
4. Designer View に HP ゲージが作成され、Root が選択され、作成完了 toast が出る。
5. Designer View / Widget Tree / Properties で内部パーツを調整する。
6. Preview HP panel で 100%, 75%, 50%, 25%, 0% や任意値を確認する。
7. `Save HP Gauge Prefab` で保存する。
8. 保存後、`Open in Asset Browser` または `Switch to Level Editor` を選ぶ。
9. Level Editor で保存済み UI Prefab を SceneView にドラッグする。
10. Canvas picker で配置先 Canvas を選ぶ。Canvas がなければ作成できる。
11. SceneView に Prefab instance が作成され、Root が選択され、Translate Gizmo が出る。
12. SceneView Gizmo または UI Alignment menu で最終位置を調整する。

重要な責務分離:

- UI Editor は Scene 上の最終配置を行わない。
- UI Editor の Designer View は Prefab 内部レイアウト編集専用である。
- Level Editor / SceneView が Scene / Canvas への配置と最終位置調整を担当する。

## 3. UX Principles

### 2.1 Visible Before Editable

操作できる場所は、操作する前に視覚的に分かること。

必須:

- Hover outline。
- Selected outline。
- Resize handle。
- Entity name label。
- Cursor change。
- Snap guide。

禁止:

- 見えない `InvisibleButton` だけで操作させる。
- 重なった要素の選択を運任せにする。

### 2.2 One Action, One Undo

ユーザーの 1 操作は Undo 1 回で戻ること。

必須:

- Designer drag move は mouse release で 1 action。
- Designer resize は mouse release で 1 action。
- Properties DragFloat / Slider / ColorEdit は edit end で 1 action。

禁止:

- Drag 中に毎フレーム Undo action を積む。

### 2.3 Editor State Must Not Pollute Runtime Data

Preview や hover や selection は authoring data / runtime data を汚さないこと。

必須:

- Preview HP は `UIEditorHPPreviewState` に保存する。
- `HPGaugeBindingComponent::currentHP` / `targetRatio` / `displayedRatio` / `delayedRatio` / `targetValid` を Preview 操作で書き換えない。

### 2.4 UI Editor Creates Prefabs, Level Editor Places Them

UI Editor は Prefab の中身を作る場所であり、Scene に置く場所ではない。

必須:

- UI Editor に最終配置 preset を戻さない。
- 保存後の配置導線は Level Editor / SceneView へ向ける。
- SceneView 側に Canvas picker / alignment / safe area snap を置く。

## 4. Screen Layout

### 3.1 UI Editor Workspace

推奨 layout:

```text
+---------------------------------------------------------------------+
| Toolbar: Template... | Save | Apply | Preview 50% | Zoom 100%       |
+----------------+--------------------------------------+-------------+
| Palette        | Designer View                        | Properties  |
| - Templates    | - authoring canvas                    | - Summary   |
| - Parts        | - hover/selected labels               | - Preview   |
| - Saved Prefab | - move/resize handles                 | - Sections  |
+----------------+--------------------------------------+-------------+
| Widget Tree / Status / Prefab Path                                  |
+---------------------------------------------------------------------+
```

### 3.2 Toolbar

Toolbar contents:

- `Template...`
- `Save HP Gauge Prefab`
- `Apply`
- `Revert`
- `Unpack`
- Preview ratio quick selector
- Zoom value
- Zoom reset
- Grid size selector
- Snap toggle
- Safe Area toggle
- Grid toggle

Toolbar must show:

- Current selected entity name.
- Current prefab target root.
- Saved / Dirty state.

### 3.3 Palette

Palette sections:

- Templates
- Parts
- Saved Prefabs

Saved Prefabs are shown for discovery only. Placement is done from Level Editor / Asset Browser.

## 5. Requirement IDs

### TPL: Template Picker

TPL-01:
`Template...` opens a Template Picker, not a tiny anonymous popup.

TPL-02:
Template Picker contains only:

- `Player HP`
- `Boss HP`

TPL-03:
Each template is shown as a card with:

- Name.
- Use case.
- Recommended Level placement.
- Default size.
- Generated hierarchy.
- Small visual preview.

TPL-04:
Creating a template selects the root entity.

TPL-05:
Creating a template frames the root in Designer View.

TPL-06:
Creating a template displays a toast:

```text
Player HP template created. Edit parts, then save as prefab.
```

### DSV: Designer View

DSV-01:
Designer View draws hover outline for the entity under the cursor.

DSV-02:
Designer View draws selected outline for all selected entities.

DSV-03:
Selected entity label appears at the top-left of its bounds.

DSV-04:
HP Gauge Root draws a visible header strip so it can be selected even when children overlap it.

DSV-05:
Resize handles are at least 14px square.

DSV-06:
Cursor changes by operation:

- Move body: move cursor.
- Resize handle: resize cursor.
- Pan: hand-like or move cursor.

DSV-07:
When multiple entities overlap at click position, a candidate picker appears.

DSV-08:
`Alt + click` cycles through overlap candidates without opening the candidate picker.

DSV-09:
Dragging selected body moves the entity.

DSV-10:
Dragging resize handle changes size.

DSV-11:
Dragging shows ghost outline.

DSV-12:
Snap guide appears when snapping.

DSV-13:
Snap grid sizes:

- 10
- 20
- 40
- 80
- 120

DSV-14:
Modifier behavior:

- `Alt`: temporarily disable snap.
- `Shift`: 10px step.
- `Ctrl`: 1px step.

DSV-15:
Designer View uses `UIRectEvaluator` for hierarchy-aware bounds.

DSV-16:
Designer View drawing order matches runtime draw order:

1. `sortingLayer`
2. `orderInLayer`
3. `z`
4. `entity`

### NAV: Pan / Zoom / Frame

NAV-01:
Mouse wheel zooms around cursor.

NAV-02:
Middle mouse drag pans.

NAV-03:
`Space + left drag` pans.

NAV-04:
`F` frames selected widget.

NAV-05:
`Home` frames the whole authoring canvas.

NAV-06:
Toolbar shows zoom percentage.

NAV-07:
Toolbar has zoom reset.

NAV-08:
Zoom is clamped to a useful range:

- Min: 10%
- Max: 400%

### WGT: Widget Tree

WGT-01:
Widget Tree supports selection.

WGT-02:
Widget Tree shows type badges:

- Canvas
- Gauge
- Image
- Fill
- Text

WGT-03:
Widget Tree supports inline rename.

WGT-04:
Right-click context menu contains:

- Rename
- Delete
- Duplicate
- Move Up
- Move Down
- Save HP Gauge Prefab
- Frame in Designer

WGT-05:
Visibility toggle is available per entity.

WGT-06:
Lock toggle is available per entity.

WGT-07:
Drag and drop reorders siblings.

WGT-08:
Drag and drop reparents entities.

WGT-09:
Prefab instance locked child cannot be modified directly.

WGT-10:
When an operation is blocked by prefab lock, a tooltip explains why and suggests `Unpack`.

### PRP: Properties Panel

PRP-01:
Properties starts with Selection Summary.

Selection Summary shows:

- Entity name.
- Type.
- Parent.
- HP Gauge root.
- Prefab root.
- Dirty / Override state.

PRP-02:
Preview HP panel is always visible when selected entity belongs to an HP Gauge.

PRP-03:
Sections are collapsible:

- Rect Transform
- Canvas Item
- HP Gauge Binding
- Image
- Fill
- Text
- HP Text Binding
- Prefab

PRP-04:
Texture path field is a visible drag/drop target.

PRP-05:
Font path field is a visible drag/drop target.

PRP-06:
Empty asset path fields show placeholder text.

PRP-07:
Fill section includes a mini preview bar.

PRP-08:
Text section includes live text preview.

PRP-09:
Properties edit sessions group continuous edits into one Undo action.

### HPV: HP Preview

HPV-01:
Preview panel has quick buttons:

- 100%
- 75%
- 50%
- 25%
- 0%

HPV-02:
Preview panel has ratio slider.

HPV-03:
Preview panel has current HP input.

HPV-04:
Preview panel has max HP input.

HPV-05:
Preview panel has delayed damage ratio slider.

HPV-06:
Preview panel has `Play Damage Preview`.

HPV-07:
Fill preview uses preview ratio.

HPV-08:
DamagePreview uses delayed preview ratio.

HPV-09:
HP Text preview uses preview current / max HP.

HPV-10:
Preview never writes runtime values to `HPGaugeBindingComponent`.

### PFB: Prefab Save / Apply

PFB-01:
`Save HP Gauge Prefab` validates the target before saving.

Validation:

- Root has `HPGaugeBindingComponent`.
- At least one `HPGaugeFillComponent` exists under root.
- Root has `RectTransformComponent`.
- Root has `CanvasItemComponent`.
- Root has `TransformComponent`.

PFB-02:
First save opens Save dialog.

PFB-03:
Existing Prefab instance exposes:

- `Apply`
- `Revert`
- `Unpack`
- `Save As`

PFB-04:
After save, toast appears:

```text
Saved HP Gauge Prefab: <path>
```

PFB-05:
After save, UI shows:

- `Open in Asset Browser`
- `Switch to Level Editor`

PFB-06:
Saved path is visible and copyable.

PFB-07:
Save / Apply / Revert / Unpack display success or failure toast.

### LPL: Level Placement

LPL-01:
Level Editor has UI Prefab Placement flow.

LPL-02:
Dropping a UI Prefab into SceneView uses 2D canvas point when SceneView is in 2D mode.

LPL-03:
Dropping UI Prefab shows bounds preview, not only a point marker.

LPL-04:
Dropping UI Prefab opens Canvas picker if multiple Canvas candidates exist.

LPL-05:
If no Canvas exists, Canvas picker offers `Create Canvas`.

LPL-06:
Placed UI Prefab becomes child of selected Canvas.

LPL-07:
Placed UI Prefab root is selected.

LPL-08:
Gizmo operation becomes Translate.

LPL-09:
SceneView 2D toolbar contains UI alignment menu:

- Top Left
- Top Center
- Top Right
- Middle Left
- Middle Right
- Bottom Left
- Bottom Center
- Bottom Right

LPL-10:
Alignment uses safe area when Safe Area is enabled.

LPL-11:
Alignment uses canvas bounds when Safe Area is disabled.

### SGZ: SceneView Gizmo Fine Control

SGZ-01:
SceneView 2D toolbar shows selected UI mini editor.

Mini editor fields:

- X
- Y
- Width
- Height

SGZ-02:
Arrow keys move selected UI:

- Arrow: 1px
- Shift + Arrow: 10px
- Ctrl + Arrow: 0.1px

SGZ-03:
Safe area snap is available.

SGZ-04:
Canvas edge snap is available.

SGZ-05:
After Gizmo move, Undo restores one complete move operation.

### MSG: Status / Toast

MSG-01:
UI Editor has status/toast line.

MSG-02:
Messages appear for:

- Template created.
- Part created.
- Prefab saved.
- Prefab applied.
- Prefab reverted.
- Prefab unpacked.
- Operation blocked.
- Invalid save target.

MSG-03:
Toast includes action result and next action where useful.

Example:

```text
Saved. Open in Asset Browser or switch to Level Editor to place it.
```

## 6. Keyboard Shortcuts

UI Editor shortcuts:

| Shortcut | Action |
|---|---|
| Delete | Delete selected widget |
| Ctrl + D | Duplicate selected widget |
| Ctrl + Z | Undo |
| Ctrl + Y | Redo |
| Ctrl + S | Save HP Gauge Prefab / Apply existing prefab |
| Ctrl + Shift + S | Save As |
| F | Frame selected |
| Home | Frame canvas |
| Esc | Clear selection / close popup |
| Arrow | Move selected 1px |
| Shift + Arrow | Move selected 10px |
| Ctrl + Arrow | Move selected 0.1px |
| Space + Drag | Pan Designer View |
| Alt + Click | Cycle overlapping selection |

SceneView 2D shortcuts:

| Shortcut | Action |
|---|---|
| W | Translate gizmo |
| E | Rotate gizmo |
| R | Scale gizmo |
| Arrow | Move selected UI 1px |
| Shift + Arrow | Move selected UI 10px |
| Ctrl + Arrow | Move selected UI 0.1px |

## 7. Implementation Phases

### Phase 1: Make Interaction Visible

Goal:

Designer View must clearly communicate what can be touched.

Tasks:

- Hover outline.
- Selected label.
- Root header strip.
- Larger resize handles.
- Cursor change.
- Ghost outline during drag.
- Status message system.

Acceptance:

- Root / Background / Fill / DamagePreview / Text can be visually distinguished.
- User can tell which entity is hovered.
- User can tell which entity is selected.
- Resize affordance is visible without guessing.

### Phase 2: Selection Reliability

Goal:

Overlapping UI elements must be selectable reliably.

Tasks:

- Hit candidate collection using display order.
- Candidate picker popup.
- `Alt + click` cycle.
- Tree selection sync.
- Frame selected after creation.

Acceptance:

- Fill / DamagePreview / Background overlap can be selected intentionally.
- Tree and Designer selection always match.
- Template creation selects and frames root.

### Phase 3: View Navigation

Goal:

Large and small UI can be edited comfortably.

Tasks:

- Wheel zoom.
- Cursor-centered zoom.
- Middle drag pan.
- Space + drag pan.
- `F` frame selected.
- `Home` frame canvas.
- Zoom reset.
- Grid size selector.

Acceptance:

- Boss HP can be zoomed out and edited.
- HP Text can be zoomed in and edited.
- User can return to full canvas instantly.

### Phase 4: Tree Editing

Goal:

Widget Tree becomes a real editing surface.

Tasks:

- Context menu.
- Inline rename.
- Delete.
- Duplicate.
- Move Up / Down.
- Visibility toggle.
- Lock toggle.
- Drag reorder.
- Drag reparent.

Acceptance:

- Basic widget management can be done without Inspector.
- Overlapped parts can be selected and managed from Tree.

### Phase 5: Properties Usability

Goal:

Properties becomes scannable and safe.

Tasks:

- Selection summary.
- Collapsible sections.
- Preview HP panel always visible for HP Gauge.
- Asset drag/drop fields.
- Fill mini preview.
- Text mini preview.
- Properties edit session.

Acceptance:

- Important properties are not buried.
- Preview does not write runtime fields.
- Properties drag edits produce one Undo action.

### Phase 6: Prefab Save Flow

Goal:

Saving leads naturally to Level placement.

Tasks:

- Save validation.
- Save dialog.
- Save As.
- Save toast.
- Open in Asset Browser.
- Switch to Level Editor.
- Saved path display.

Acceptance:

- User can name the prefab.
- User can find it after saving.
- User knows the next step is Level Editor placement.

### Phase 7: Level Placement Flow

Goal:

Saved UI Prefab can be placed into the Scene / Canvas cleanly.

Tasks:

- UI Prefab bounds drop preview.
- Canvas picker.
- Create Canvas option.
- Parent placed Prefab under Canvas.
- Select root after placement.
- SceneView UI alignment menu.
- Safe area snap.
- Canvas edge snap.
- Selected UI mini editor.

Acceptance:

- UI Prefab placement target is explicit.
- Placed Prefab is selected and ready for Gizmo adjustment.
- Top Left / Top Center / Top Right / Middle Left / Middle Right / Bottom Left / Bottom Center / Bottom Right alignment works.

## 8. Acceptance Test Suite

### 7.1 Template Creation

1. Open UI Editor.
2. Click `Template...`.
3. Confirm `Player HP` and `Boss HP` cards are shown.
4. Select `Player HP`.
5. Confirm root is selected.
6. Confirm Designer View frames the root.
7. Confirm toast appears.

### 7.2 Designer Hover / Selection

1. Hover Root.
2. Confirm root header strip and hover outline.
3. Hover Fill.
4. Confirm Fill label.
5. Click overlap area.
6. Confirm candidate picker can choose Background / DamagePreview / Fill.
7. Alt-click repeatedly.
8. Confirm selected candidate cycles.

### 7.3 Designer Move / Resize

1. Select Fill.
2. Drag body.
3. Confirm ghost outline and snap guide.
4. Release mouse.
5. Undo once.
6. Confirm position returns.
7. Drag resize handle.
8. Undo once.
9. Confirm size returns.

### 7.4 Pan / Zoom

1. Wheel zoom in.
2. Confirm zoom is cursor-centered.
3. Middle drag pan.
4. Press `F`.
5. Confirm selected widget is framed.
6. Press `Home`.
7. Confirm canvas is framed.

### 7.5 Widget Tree Editing

1. Rename `HP_Text` from Tree.
2. Duplicate `Image`.
3. Move duplicate up/down.
4. Toggle visibility.
5. Delete duplicate.
6. Undo delete.
7. Confirm duplicate returns.

### 7.6 Properties and Preview

1. Select Root.
2. Confirm Preview HP panel is visible.
3. Set ratio to 50%.
4. Confirm Fill is half.
5. Confirm Text shows `50 / 100`.
6. Save Prefab.
7. Confirm runtime fields are not saved from preview state.

### 7.7 Prefab Save

1. Click `Save HP Gauge Prefab`.
2. Save dialog appears.
3. Enter name.
4. Save.
5. Toast appears.
6. Click `Open in Asset Browser`.
7. Confirm saved prefab is selected or visible.
8. Click `Switch to Level Editor`.
9. Confirm Level Editor becomes active.

### 7.8 Level Placement

1. Drag saved HP Gauge Prefab from Asset Browser to SceneView.
2. Confirm bounds preview.
3. Confirm Canvas picker appears when needed.
4. Select Canvas or create one.
5. Drop.
6. Confirm Prefab root is selected.
7. Confirm Translate gizmo is active.
8. Use alignment menu `Top Left`.
9. Confirm Prefab aligns to safe area top-left.
10. Undo.
11. Confirm placement is undone.

## 9. Non-Goals

These are not required for this operability pass:

- Auto Layout / layout groups.
- Rich Text editor.
- 9-slice image editing.
- Mask editing.
- World Space UI.
- UI animation timeline.
- Button interaction authoring.
- Multi-select transform.
- Responsive breakpoint system.
- Nested prefab diff UI.

## 10. Prohibited Regressions

- Do not move Scene final placement back into UI Editor.
- Do not write Preview HP into runtime component fields.
- Do not make clickable regions invisible without hover feedback.
- Do not rely on Inspector for HP Gauge editing.
- Do not add per-frame Undo actions during drag.
- Do not place UI Prefab without an explicit Canvas decision when multiple Canvas candidates exist.
- Do not leave user with no next action after saving.

## 11. Definition of Done

This specification is complete when:

- Requirements TPL / DSV / NAV / WGT / PRP / HPV / PFB / LPL / SGZ / MSG are implemented or explicitly deferred.
- All acceptance tests in section 8 pass.
- The central proposition, Intuitive Operation, is satisfied in the happy path: create template, edit parts, preview HP, save prefab, place in Level Editor.
- `MSBuild Game.vcxproj /p:Configuration=Debug /p:Platform=x64` passes.
- UI Editor can create, edit, preview, and save Player HP and Boss HP Prefabs without using Inspector.
- Level Editor can place saved HP Gauge Prefabs into a Canvas and adjust final position in SceneView.
- Preview state does not pollute Prefab or Scene data.
- Undo behavior is one user operation per Undo action for all core operations.

## 12. Recommended First Implementation Slice

Start with the smallest slice that improves feel immediately:

1. Hover outline.
2. Selected label.
3. Larger resize handles.
4. Candidate selection popup.
5. Mouse wheel zoom.
6. Middle drag pan.
7. Preview HP panel always visible.
8. Save toast.

This slice should be implemented before Tree drag/reparent or Level alignment, because it fixes the most immediate "what am I touching?" problem.
