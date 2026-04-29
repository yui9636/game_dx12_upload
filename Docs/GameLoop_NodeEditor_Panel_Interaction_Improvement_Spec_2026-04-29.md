# GameLoop Node Editor Panel Interaction Improvement Spec

作成日: 2026-04-29

対象:

- `Source/GameLoop/GameLoopEditorPanel.h`
- `Source/GameLoop/GameLoopEditorPanel.cpp`
- `Source/GameLoop/GameLoopAsset.h`
- `Source/GameLoop/GameLoopAsset.cpp`

関連仕様:

- `Docs/GameLoop_SceneTransition_Spec_2026-04-26.md`
- `Docs/GameLoop_Loading_NodeGraph_Improvement_Spec_2026-04-29.md`
- `Docs/GameLoop_Asset_Panel_Improvement_Spec_v2_2026-04-29.md`

## 1. 背景

現状の `GameLoopEditorPanel` は、見た目として node / arrow を描画しているが、実際の編集操作の多くが右側 Inspector と List に偏っている。

これは node-based editor として不十分である。

GameLoop は scene node と transition を視覚的に接続して作る authoring system であるため、canvas 上の node / pin / arrow / label を直接クリック、ドラッグ、右クリックして編集できなければならない。

## 2. 問題点

現状の主な問題:

- node graph が編集の中心ではなく、飾りに近い
- arrow / transition を直接触る操作が弱い
- transition condition / loading policy / priority が canvas 上から編集しづらい
- node を作る、つなぐ、名前を変える、scene を割り当てる作業が Inspector 依存になっている
- right click / double click / hover / selection の feedback が足りない
- panel 全体の操作導線が node editor として成立していない

## 3. 目的

この仕様書の目的は、`GameLoopEditorPanel` を Inspector 中心 UI から、canvas-first の node-based editor に作り直すことである。

ユーザーは以下を canvas 上だけで完了できる必要がある。

- node を作る
- node を移動する
- node 名を変更する
- scenePath を設定する
- start node を指定する
- pin から pin へ drag して transition を作る
- arrow をクリックして transition を選択する
- arrow を右クリックして transition を編集する
- transition condition を追加、変更、削除する
- transition priority を変更する
- transition loading policy を設定する
- node / transition を削除、複製する

Inspector は詳細編集用の補助 UI とする。

## 4. 非目的

以下はこの改善の主目的ではない。

- `GameLoopSystem` の runtime 評価ロジック変更
- `SceneTransitionSystem` の load 実行方式変更
- `.gameloop` asset schema の大規模変更
- `collapsed` / `color` の Phase 1 導入
- external node editor library への置き換え
- Timeline / StateMachine editor との統合

## 5. 基本方針

基本方針:

- canvas 上の視覚要素は、すべて操作対象にする
- node / arrow / pin / label は hover、selected、active 状態を持つ
- Inspector でしかできない編集をなくす
- 右クリック context menu と double click editing を標準導線にする
- transition は arrow 自体を主操作対象にする
- priority は source-local priority として canvas 上にも表示、操作できるようにする
- undo / redo は Phase 2 以降でもよいが、操作単位は undo 対応しやすく分ける

## 6. Panel Layout

v1 の推奨 layout:

```text
+--------------------------------------------------------------+
| Toolbar                                                      |
+----------------------+---------------------------------------+
| Compact Outliner     | Node Graph Canvas                     |
| - Search             |                                       |
| - Nodes              |   [Title] ---> [Battle] ---> [Result] |
| - Transitions        |      ^                         |       |
|                      |      +---------- Retry --------+       |
+----------------------+---------------------------------------+
| Bottom Status / Validate / Runtime Preview                   |
+--------------------------------------------------------------+
```

右側 Inspector 固定ではなく、選択中 object の編集は以下の 2 系統にする。

- canvas 上の floating property popover
- compact inspector drawer

Inspector drawer は常時主役にしない。

## 7. 操作モデル

### 7.1 Selection

Selection 対象:

- node
- transition
- condition
- empty canvas

操作:

- node left click: node select
- arrow left click: transition select
- condition chip left click: condition select
- empty canvas left click: selection clear
- Ctrl + left click: multi select toggle
- Shift + drag: marquee select
- Delete: selected node / transition delete
- Escape: current connection / inline edit / popup cancel

Phase 1 では single select 必須、multi select は Phase 2 でよい。

### 7.2 Hover

hover 表示:

- node hover: outline を明るくする
- pin hover: pin を拡大、色変更
- arrow hover: line thickness を上げる
- arrow label hover: background を明るくする
- condition chip hover: edit 可能な見た目にする

hover している対象は tooltip を出す。

## 8. Node 操作

### 8.1 Node 作成

canvas 右クリック menu:

- Add Scene Node
- Add Title/Battle/Result Template
- Paste
- Validate From Here
- Frame All

Add Scene Node は mouse position に node を作る。

作成直後:

- node を選択する
- node name を inline edit 状態にする
- scenePath は `Data/Scenes/New.scene` を仮設定する
- Validate では未存在 file warning とする

### 8.2 Node 移動

node header または body drag で graphPos を移動する。

移動中:

- connected arrow は追従表示する
- grid snap は toolbar toggle とする
- Alt drag は duplicate move とする

Phase 1:

- single node drag 必須
- group drag は Phase 2

### 8.3 Node Inline Edit

node title double click:

- node title を canvas 上で `InputText` に切り替える
- Enter で確定
- Escape でキャンセル

scenePath badge double click:

- small path edit popup を node 直下に出す
- 入力された path は `NormalizeGameLoopScenePath` を通す
- absolute path が入った場合、`Data/` segment 以降を抽出できれば `Data/...` に正規化する
- `Data/` segment がない場合は error state を表示する

### 8.4 Node Context Menu

node 右クリック menu:

- Rename
- Set As Start
- Duplicate
- Delete
- Add Transition From This
- Add Transition To This
- Select Incoming Transitions
- Select Outgoing Transitions
- Open Scene
- Copy Scene Path

`Set As Start` は node 左上に start badge を表示する。

### 8.5 Node Visual

node 表示要素:

- name
- scenePath short label
- start badge
- incoming pin
- outgoing pin
- outgoing transition count
- validate badge

validate badge:

- error: red
- warning: yellow
- ok: green

## 9. Pin / Connection 操作

### 9.1 Pin

node は左右に pin を持つ。

- left pin: input
- right pin: output

pin は常時見えるようにする。

### 9.2 Transition 作成

output pin を left drag する。

drop 先:

- input pin 上: transition 作成
- node body 上: その node への transition 作成
- empty canvas 上: new node menu を開き、作成後に transition 接続
- Escape / right click: cancel

transition 作成直後:

- transition を選択する
- floating transition editor を arrow midpoint に開く
- condition preset を選べる状態にする

default condition:

- from node name が `Title` の場合: `InputPressed Confirm`
- to node name が `Battle` かつ from が `Result` の場合: `InputPressed Retry`
- それ以外: `None` を作らず、condition preset popup を必ず開く

Validate では condition 0 件 transition は error。

## 10. Transition / Arrow 操作

### 10.1 Arrow Hit Test

transition は arrow 本体、arrow head、label のどれをクリックしても選択できる。

hit area:

- bezier curve 周辺に十分な太さの invisible hit band を持つ
- label の rectangular hit area を持つ
- arrow head の triangular hit area を持つ

Phase 1 では厳密な curve distance でなくてもよい。

ただし label だけが押せる状態は禁止。

### 10.2 Arrow Selection

arrow selected:

- line color を強調
- line thickness を上げる
- label border を強調
- from / to node の pin を highlight

### 10.3 Arrow Double Click

arrow double click:

- floating transition editor を arrow midpoint に開く

floating transition editor:

- transition name
- from / to node
- source-local priority
- condition chips
- loading mode
- Delete / Duplicate / Reverse

この popup だけで transition の主要編集を完了できること。

### 10.4 Arrow Context Menu

arrow 右クリック menu:

- Edit Transition
- Add Condition
- Duplicate
- Reverse Direction
- Delete
- Move Priority Up
- Move Priority Down
- Set Loading: Immediate
- Set Loading: Fade
- Set Loading: Overlay

### 10.5 Priority

transition priority は従来通り `transitions` 配列順を runtime 評価順とする。

UI 表示では同じ `fromNodeId` を持つ transition だけを抜き出した source-local priority を表示する。

arrow label:

```text
#0 Confirm
#1 Cancel
```

Move Priority Up / Down:

1. 選択中 transition の `fromNodeId` を見る
2. 同じ `fromNodeId` の transition index list を作る
3. source-local 上の前後 transition と、実際の `transitions` 配列上で swap する
4. 保存順も swap 後の `transitions` 配列順をそのまま保存する

異なる `fromNodeId` の transition とは priority 操作で入れ替えない。

## 11. Condition 操作

### 11.1 Condition Chip

arrow label の下、または floating transition editor 内に condition chip を表示する。

例:

```text
[Input Confirm] [Player moved >= 1.0]
```

chip 操作:

- click: condition select
- double click: condition edit popup
- right click: condition context menu
- drag reorder: Phase 2

### 11.2 Condition Preset Menu

Add Condition menu:

- Input Confirm
- Input Cancel
- Input Retry
- Timer
- Player Moved Distance
- Actor Dead
- All Actors Dead
- Runtime Flag
- UI Button Clicked

Preset 選択後は必要 parameter を popup 内で編集できる。

### 11.3 Condition Edit Popup

condition edit popup:

- type
- actionIndex
- actorType
- threshold
- seconds
- targetName
- parameterName
- eventName

condition type に応じて必要項目だけ表示する。

## 12. Loading Policy 操作

transition floating editor と context menu から loading policy を編集できる。

表示:

- arrow label 右側に loading badge を出す

例:

```text
#0 Confirm  [Fade]
#1 Retry    [Overlay]
```

編集項目:

- mode
- fadeOutSeconds
- fadeInSeconds
- minimumLoadingSeconds
- loadingMessage
- blockInput

loading policy は transition 単位で持つ。

## 13. Toolbar

Toolbar 項目:

- New Default Loop
- Load
- Save
- Save As
- Validate
- Frame All
- Zoom 100%
- Grid Snap
- Show Validate Badges
- Show Runtime Preview

Toolbar は asset 操作と viewport 操作に限定する。

node / transition の編集 command を Toolbar に逃がさない。

## 14. Compact Outliner

Outliner は補助であり、主編集 UI ではない。

役割:

- 検索
- 選択
- asset 全体の一覧確認
- validate error / warning への jump

Outliner でしかできない編集は禁止。

## 15. Inspector Drawer

Inspector drawer は以下の用途に限定する。

- 大きな文字列編集
- validate 詳細
- runtime state debug
- 複数 condition の細かい編集

node / transition の基本操作は canvas 上で完結すること。

## 16. Keyboard Shortcut

必須 shortcut:

- Delete: selected object delete
- Ctrl+D: duplicate selected node / transition
- Ctrl+S: save
- Ctrl+Z: undo, Phase 2
- Ctrl+Y: redo, Phase 2
- F: frame selected
- A: frame all
- Escape: cancel connection / popup / inline edit
- Enter: confirm inline edit
- Space + drag: pan
- Mouse wheel: zoom
- Middle drag: pan

## 17. Visual Feedback

必要な feedback:

- hover highlight
- selected highlight
- dragging state
- connection preview
- invalid drop target color
- valid drop target color
- validate badge
- unsaved changes marker
- runtime current node highlight
- runtime pending transition highlight

runtime current node:

- play 中は current node を青系 border で表示する

pending transition:

- transition requested 中は該当 arrow を点滅、または accent color で表示する

## 18. Data / Schema 影響

Phase 1 では asset schema 追加を最小にする。

既存で使用するもの:

- `GameLoopNode::id`
- `GameLoopNode::name`
- `GameLoopNode::scenePath`
- `GameLoopNode::graphPos`
- `GameLoopTransition::id`
- `GameLoopTransition::fromNodeId`
- `GameLoopTransition::toNodeId`
- `GameLoopTransition::conditions`
- `GameLoopTransition::loadingPolicy`

Phase 1 では入れない:

```cpp
bool collapsed = false;
uint32_t color = 0;
```

必要な editor-only state:

- hoveredNodeId
- hoveredTransitionId
- hoveredPin
- activeDragKind
- selected ids
- inlineEditTarget
- contextMenuTarget
- floatingEditorTarget
- marqueeRect, Phase 2

editor-only state は `.gameloop` に保存しない。

## 19. Path Validate

scenePath 仕様は既存改善仕様に合わせる。

- `.gameloop` に保存する path は `Data/...`
- absolute path は保存しない
- file picker / drag drop で absolute path が来た場合、`Data/` segment 以降を抽出できれば `Data/...` に正規化する
- `Data/` segment がない absolute path は Validate error
- `Data/` 外 path は Validate error
- `../` parent traversal は Validate error
- `Data/...` として正しいが file が存在しない場合は Warning

node 上では warning badge を表示する。

## 20. 実装フェーズ

### Phase 0: 緊急 UX 修正

目的:

node graph が飾りではなく、最低限触れる editor になること。

実装:

- arrow 本体 / arrow head / label click で transition select
- arrow right click menu
- arrow double click floating transition editor
- node right click menu
- node title double click rename
- output pin drag to input pin で transition 作成
- Delete key で selected delete
- hover highlight
- selected highlight
- condition preset popup

完成条件:

- List / Inspector を使わずに `Title -> Battle -> Result -> Battle` を作れる
- arrow を直接クリックして condition を変更できる
- arrow を右クリックして Delete / Duplicate / Reverse / Priority Up Down ができる

### Phase 1: Canvas-first Editor

実装:

- floating node editor
- floating transition editor
- condition chip
- loading badge
- validate badge
- frame selected / frame all
- scenePath badge edit
- drop on empty canvas to create node and connect
- grid snap

完成条件:

- node / transition の主要編集が canvas 上で完結する
- Inspector drawer を閉じた状態でも GameLoopAsset を作成、保存、Validate できる

### Phase 2: Production Editing

実装:

- multi select
- marquee select
- group move
- undo / redo
- copy / paste
- drag reorder condition chips
- transition reroute handle, 必要なら
- search jump

完成条件:

- 10 node / 20 transition 程度の GameLoop を破綻なく編集できる

### Phase 3: Polish

実装:

- minimap
- auto layout
- node color
- collapsed node
- comment frame
- graph bookmarks

`collapsed` / `color` はここで再検討する。

## 21. Validate 表示

Validate 結果は outliner だけでなく、canvas 上にも出す。

node badge:

- scenePath empty
- scenePath outside Data
- scene file missing warning

transition badge:

- condition empty
- invalid from / to
- invalid condition parameter
- duplicate source-local priority は通常発生しないが、id 重複は error

クリックすると該当 node / transition を選択し、必要なら floating editor を開く。

## 22. 受け入れ条件

v1 受け入れ条件:

- blank asset から canvas 右クリックで node を作れる
- node title を double click して rename できる
- node scenePath を node 上から編集できる
- output pin drag で transition を作れる
- arrow 本体をクリックして transition を選択できる
- arrow 右クリック menu から transition を削除できる
- arrow double click で transition editor が開く
- transition editor から condition を追加、変更、削除できる
- transition editor から loading policy を変更できる
- source-local priority を arrow menu から変更できる
- Delete key で selected node / transition を削除できる
- Validate error / warning が canvas 上の badge として見える
- Inspector を使わずに標準 v0 loop を作成できる
- Debug x64 build が通る

## 23. 禁止事項

禁止:

- node graph を表示専用に戻すこと
- arrow selection を label click だけに依存すること
- transition 編集を Inspector 専用にすること
- node 作成や transition 作成を Outliner 専用にすること
- canvas 右クリック menu を空にすること
- Validate 結果を panel 下部の text だけに閉じ込めること
- Phase 1 で `collapsed` / `color` を schema に入れて主作業を膨らませること
- runtime `GameLoopSystem` と editor UI 操作を混ぜること

## 24. 実装メモ

`ImGui::InvisibleButton` で全操作を作る場合でも、以下の層を分ける。

1. canvas background hit
2. transition hit
3. node body hit
4. pin hit
5. popup / floating editor

優先順位:

```text
popup > pin > node > transition > canvas
```

transition hit は、少なくとも以下を別 hit area にする。

- label rect
- arrow head rect
- curve bounding band

curve bounding band は Phase 0 では大きめの AABB でもよい。

ただし arrow が長い場合に midpoint label しか押せない状態は不可。

## 25. 最初に直すべき順序

最優先:

1. arrow の click / right click / double click を成立させる
2. node right click menu を成立させる
3. output pin drag to input pin を確実に成立させる
4. floating transition editor を作る
5. condition preset popup を作る
6. Delete key / Duplicate / Reverse / Priority Up Down を canvas 操作へ出す

これが終わるまで、Outliner / Inspector の追加機能を増やさない。

