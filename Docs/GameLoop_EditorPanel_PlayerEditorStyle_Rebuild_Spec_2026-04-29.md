# GameLoop EditorPanel PlayerEditor-Style Rebuild Spec

## 0. v2 Extreme Usability Revision

この章を最優先仕様とする。

前版の不足は、機能項目の不足ではなく「操作思想」の不足である。

高度な editor とは、button が多い editor ではない。操作項目を雑に増やしても、作業速度は上がらない。GameLoopEditor は、常時表示 button を増やすのではなく、対象の近くで、少ない gesture で、予測可能に編集できる canvas-first editor として作る。

UE Blueprint / Behavior Tree / StateTree、Unity Shader Graph / Visual Scripting / Animator Controller の良い部分を参考にする。ただし模倣で終わらせない。GameLoop は scene transition 専用 editor なので、汎用 node editor よりも短い操作、低い表示密度、壊れない text layout を優先する。

### 0.1 Core Doctrine

必須方針:

- Graph が主役。Outliner / Properties / Toolbar は補助。
- 常時 button を増やさない。操作は click / drag / right click / double click / shortcut に寄せる。
- command は対象の近くに出す。node 操作は node 近く、transition 操作は arrow 近く。
- 初期画面で読めないものは不合格。
- node が動かない editor は不合格。
- transition が簡単に作れない editor は不合格。
- 名前や path 表示が崩れる editor は不合格。

### 0.2 Button Budget

button を増やさないため、表示面ごとの上限を固定する。

Toolbar:

- permanent visible button は最大 3 個
- menu label は最大 4 個
- `Add Node` / `Add Transition` / condition preset / loading preset を常時置かない

Graph panel:

- normal state の permanent button は 0 個
- empty state のみ最大 2 個

Node:

- node 内に常時 button を置かない
- node 内に許可されるのは label / badge / pin のみ

Transition:

- arrow 上に常時 button を置かない
- arrow 上に許可されるのは label / condition chip / badge のみ

Properties:

- destructive button は下部にまとめる
- main editing は field / contextual command 中心にする

この budget を超える UI は、原則として不合格とする。

### 0.3 Zero-Button Happy Path

Toolbar を使わず、canvas だけで標準 loop を作れること。

手順:

1. graph empty area を right click
2. `Title` を選ぶ
3. Title node の output から右へ drag
4. empty area で release
5. quick create popup で `Battle` を選ぶ
6. Battle node の output から右へ drag
7. empty area で release
8. quick create popup で `Result` を選ぶ
9. Result node の output から Battle node body へ drag
10. condition preset `Retry` を選ぶ
11. Result node の output から Title node body へ drag
12. condition preset `Cancel` を選ぶ

この流れで以下が自動設定される。

- node name
- scenePath
- graph position
- transition name
- default condition
- source-local priority

Outliner と Properties を使わない。

### 0.4 Node Move Requirements

node は必ず自然に drag で動く。

合格条件:

- node body のどこを掴んでも移動できる
- pin / inline edit / context menu 操作中は誤移動しない
- drag 中に cursor が node 外へ出ても移動が継続する
- transition は drag 中も毎 frame 追従する
- release 後に `graphPos` が保存対象へ反映される
- zoom 中も `delta / zoom` で自然な移動量になる

不合格:

- node の狭い header 部分しか掴めない
- click しただけで微妙に位置がずれる
- drag と selection が競合する
- graph zoom によって移動量が暴れる

### 0.5 Edge Creation Requirements

transition 作成は pin を精密に狙わせない。

合格条件:

- output pin から target node body へ drop できる
- output pin から input pin へ drop できる
- output pin から empty canvas へ drop すると quick create node popup が出る
- quick create で node を選ぶと node 作成と transition 作成が一度に完了する
- drag 中は valid target が highlight される
- invalid target は赤系表示になる
- transition 作成直後に condition preset を近くで選べる

不合格:

- input pin だけを正確に狙わないと transition が作れない
- drop 失敗時に何が起きたか分からない
- transition 作成後、condition 設定導線が遠い

### 0.6 Edge Selection Requirements

arrow は簡単に選択できなければならない。

合格条件:

- bezier / line 近傍を click して選択できる
- arrow head を click して選択できる
- label を click して選択できる
- selected edge は太く、明るくなる
- selected edge の from / to node pin が highlight される
- hover している edge が視覚的に分かる

不合格:

- label しか click できない
- hit area が細すぎる
- hover している edge が分からない

### 0.7 Text And Label Integrity

名前表示が崩れる editor は不合格である。

Node text:

- node name は 1 行
- scenePath は 1 行
- long scenePath は middle ellipsis
- badge と text は重ならない
- zoom < 0.55 では scenePath を非表示
- zoom < 0.40 では node name のみ表示

middle ellipsis 例:

```text
Data/Scenes/.../Battle.scene
```

Transition label:

- label は edge midpoint から normal 方向へ offset する
- reverse edge がある場合は offset direction を逆にする
- same source から複数 edge がある場合は lane index を持つ
- label max width を持つ
- condition summary が長い場合は compact 表示にする

compact 例:

```text
#0 Confirm  C:2
```

Condition chip:

- chip は最大 2 個まで表示
- 3 個以上は `+N` badge
- chip が node rect と重なる場合は chip を非表示にして `C:N` 表示へ落とす

### 0.8 Smart Defaults

操作を減らすため、GameLoopEditor は smart defaults を持つ。

Node creation:

```text
Title  -> name Title,  scenePath Data/Scenes/Title.scene
Battle -> name Battle, scenePath Data/Scenes/Battle.scene
Result -> name Result, scenePath Data/Scenes/Result.scene
Custom -> name Scene,  scenePath Data/Scenes/New.scene
```

Transition defaults:

```text
Title  -> Battle : Input Confirm
Result -> Battle : Input Retry
Result -> Title  : Input Cancel
Battle -> Result : ActorMovedDistance Player 1.0
```

Auto position:

- empty canvas drop は mouse release position を基準にする
- source node と重なる場合は右方向へ自動 offset
- 既存 node と重なる場合は spiral search で空き位置へ置く
- node を作った瞬間に重なることは禁止

### 0.9 Minimal Command Surface

操作入口は以下に限定する。

1. left click / drag
2. right click context
3. double click edit
4. keyboard shortcut
5. command palette, Phase 2

常時 button で操作を並べる方式は禁止する。

Context menu:

- top level は最大 8 item
- destructive item は最後
- separator は最大 2 個
- submenu は最大 1 階層

Shortcut:

```text
Delete    delete selected
F         frame selected
A         frame all
Ctrl+S    save
Escape    cancel / close
Enter     confirm inline edit
```

Phase 2:

```text
Ctrl+Z    undo
Ctrl+Y    redo
Ctrl+D    duplicate
Ctrl+P    command palette
```

### 0.10 UE / Unity から採用すること

UE から採用:

- node body drag の自然さ
- pin drag connection
- wire preview
- right click graph menu
- selected object が details panel に出る
- graph pan / zoom の高速さ

UE から採用しない:

- 初期段階で巨大な node palette を常設すること
- 複雑な pin type system
- GameLoop に不要な汎用 graph 機能

Unity から採用:

- Inspector は selection-based
- graph view は canvas-first
- search / create menu は popup 化
- asset path は project relative
- validate 状態を authoring 中に見せる

Unity から採用しない:

- Inspector 依存の authoring
- object list 中心の graph editing
- foldout を増やして編集導線を隠すこと

GameLoopEditor が超えるべき点:

- scenePath を node 上で常時確認できる
- condition を edge 上で短く読める
- default loop を数 drag で作れる
- Data/ path validate が canvas 上で即見える
- missing scene は warning として authoring を止めない

### 0.11 Usability Test Cases

30 秒テスト:

- 新規 GameLoop を開く
- Title node を作る
- Battle node を作る
- Title -> Battle transition を作る
- condition が Confirm であることを確認する
- Toolbar button は使わない

5 分テスト:

```text
Title -> Battle -> Result
Result -> Battle
Result -> Title
```

条件:

- node を 3 回以上 drag して配置調整できる
- arrow を直接 click して選択できる
- edge condition を 2 回以上変更できる
- label / chip が読める状態を保つ

破綻テスト:

- node name 40 文字
- scenePath 120 文字
- condition 5 件
- transition 20 件
- reverse edge 4 組

期待:

- text は clip / ellipsis
- chip は compact
- node と label は重ならない
- graph fit で全体が見える

### 0.12 Updated Priority

実装優先順位を以下に更新する。

1. DockSpace 化
2. Graph panel を最大領域にする
3. bottom fixed inspector 削除
4. Toolbar を File/Create/View menu へ縮退
5. FitGraphToContent 初期実行
6. default node position / auto layout 修正
7. node text clip / ellipsis
8. transition label lane / compact chip
9. node drag の安定化
10. edge creation の target 緩和
11. edge hit test の拡大
12. floating editor のサイズ制御
13. validate badge overlay
14. 30 秒テスト
15. 破綻テスト

button や menu item を増やす作業は、上記 1-15 が終わるまで禁止する。

作成日: 2026-04-29

対象:

- `Source/GameLoop/GameLoopEditorPanel.h`
- `Source/GameLoop/GameLoopEditorPanel.cpp`
- `Source/GameLoop/GameLoopAsset.h`
- `Source/GameLoop/GameLoopAsset.cpp`

参照した実装:

- `Source/PlayerEditor/PlayerEditorPanel.h`
- `Source/PlayerEditor/PlayerEditorPanel.cpp`
- `Source/PlayerEditor/PlayerEditorPanelInternal.h`
- `Source/PlayerEditor/PlayerEditorPanelInternal.cpp`
- `Source/PlayerEditor/PlayerEditorStateMachinePanel.cpp`
- `Source/PlayerEditor/PlayerEditorInspectorPanel.cpp`
- `Source/PlayerEditor/PlayerEditorViewportPanel.cpp`

## 1. 背景

現在の `GameLoopEditorPanel` は、node graph を名乗っているが、初期画面の時点で以下の問題が出ている。

- graph canvas が狭い
- node が重なって読めない
- transition label と condition chip が重なっている
- outliner / inspector / validate / runtime が同じ panel 内に詰め込まれている
- node を動かしにくい
- panel 自体を dock / move できない
- toolbar button が多く、主操作の邪魔になっている
- Inspector 中心で、canvas 上の編集体験になっていない

これは node editor として不合格である。

PlayerEditor 側は `PlayerEditorPanel::DrawInternal` で DockSpace root を作り、`BuildDockLayout` で Viewport / StateMachine / Properties / Timeline / Skeleton などを独立 dock window として配置している。

GameLoopEditor もこの方式に寄せる。

## 2. 目的

GameLoopEditor を、単一 window に部品を詰める UI から、PlayerEditor 方式の dockable editor workspace に作り直す。

最終的な目的:

- node graph を主画面にする
- inspector は補助に下げる
- 初期表示で node / transition が重ならない
- graph canvas 上で node を移動できる
- graph canvas 上で transition を作成、選択、編集、削除できる
- outliner / properties / validate / runtime は dockable panel に分離する
- toolbar は icon + menu 中心に整理し、ボタンの横並び地獄をやめる

## 3. 非目的

この仕様で扱わないこと:

- `GameLoopSystem` の runtime evaluation 変更
- `SceneTransitionSystem` の load 実行順変更
- `.gameloop` asset schema の大規模変更
- external node editor library への全面置き換え
- `collapsed` / `color` の Phase 1 導入
- Timeline / StateMachine / PlayerEditor の統合

## 4. PlayerEditor から採用する設計

### 4.1 DockSpace Root

PlayerEditor は root window に DockSpace を作り、その中へ sub-window を dock している。

GameLoopEditor も同じにする。

必要な構造:

```cpp
void GameLoopEditorPanel::Draw(...)
{
    DrawInternal(..., HostMode::Window);
}

void GameLoopEditorPanel::DrawInternal(...)
{
    DrawToolbar();
    ImGui::DockSpace(dockId, ...);
    if (m_needsLayoutRebuild) {
        BuildDockLayout(dockId);
    }

    DrawGraphPanel();
    DrawOutlinerPanel();
    DrawPropertiesPanel();
    DrawValidatePanel();
    DrawRuntimePanel();
}
```

GameLoopEditor の root window 内に巨大な `BeginChild` を並べる方式は禁止する。

### 4.2 BuildDockLayout

PlayerEditor の `BuildDockLayout` は以下の考え方で有効である。

- center を主作業領域にする
- left に list / tree
- right に properties
- bottom に timeline / validate のような長い情報
- それぞれを dock window にして移動可能にする

GameLoopEditor の初期 layout:

```text
+----------------------------------------------------------------+
| Toolbar / Menu                                                  |
+----------------------+---------------------------+-------------+
| Outliner             | Graph                     | Properties  |
| 20%                  | 60%                       | 20%         |
|                      |                           |             |
+----------------------+---------------------------+-------------+
| Validate / Runtime                                              |
| 24% height                                                      |
+----------------------------------------------------------------+
```

ただし Graph は常に最大領域を取る。

### 4.3 StateMachine Graph Pane

PlayerEditor の `DrawStateMachinePanel` は list pane と graph pane を分け、splitter で list width を調整できる。

GameLoop でも Outliner は補助 pane とし、Graph を主領域にする。

採用するもの:

- resizable left pane
- graph pane with full available size
- right drag pan
- middle drag pan
- mouse wheel zoom
- Fit button
- graph background context menu
- empty graph action button
- persistent gesture hints

### 4.4 Properties Panel

PlayerEditor の `DrawPropertiesPanel` は `SelectionContext` に応じて表示内容を切り替える。

GameLoopEditor もこれを採用する。

`Properties` は現在選択中のものだけを扱う。

- None
- Node
- Transition
- Condition
- Runtime
- ValidateMessage

bottom inspector のような巨大固定領域は禁止する。

### 4.5 Viewport Panel の余白ゼロ思想

PlayerEditorViewportPanel は viewport を主役にするため、window padding を 0 にしている。

GameLoopGraphPanel も同じにする。

Graph panel 内には余計な `Child` border や padding を入れない。

## 5. 新しい全体構成

GameLoopEditorPanel は以下の sub panel に分割する。

```text
GameLoopEditorPanel
  DrawInternal
  DrawToolbar
  BuildDockLayout
  DrawGraphPanel
  DrawOutlinerPanel
  DrawPropertiesPanel
  DrawValidatePanel
  DrawRuntimePanel
```

内部 helper:

```text
DrawGraphCanvas
DrawGraphGrid
DrawGraphNodes
DrawGraphTransitions
DrawNode
DrawTransition
DrawNodeContextMenu
DrawTransitionContextMenu
DrawCanvasContextMenu
DrawFloatingNodeEditor
DrawFloatingTransitionEditor
```

## 6. 初期画面仕様

初期画面では以下を満たすこと。

- Graph panel が最も広い領域を占める
- Outliner は左 20% 程度
- Properties は右 20% 程度
- Validate / Runtime は下 dock または tab
- bottom 固定 inspector は置かない
- Toolbar は 1 行に収まる
- node は自動 fit される
- transition label は node と重ならない
- condition chip は transition label と重ならない

初回表示時:

```cpp
m_graphFitRequested = true;
```

を立て、Graph panel の actual size が分かったタイミングで `FitGraphToContent` を実行する。

## 7. Toolbar 仕様

現在のような text button 横並びは禁止する。

Toolbar は以下に整理する。

```text
[File ▼] [Create ▼] [View ▼] [Validate] [Play Sync Toggle]    Current: Main.gameloop
```

File menu:

- New Default Loop
- Load
- Save
- Save As

Create menu:

- Add Scene Node
- Add Title/Battle/Result Template
- Add Transition From Selected

View menu:

- Fit Graph
- Zoom 100%
- Rebuild Layout
- Show Validate Panel
- Show Runtime Panel

Toolbar に置いてよい direct button:

- Validate
- Save dirty indicator

Toolbar に置かないもの:

- Add Node
- Add Transition
- condition preset 群
- loading preset 群

これらは graph context / transition context / properties に置く。

## 8. Dock Panels

### 8.1 Graph Panel

window title:

```cpp
ICON_FA_DIAGRAM_PROJECT " GameLoop Graph##GLE"
```

Graph panel が主作業領域である。

Graph panel の責務:

- canvas draw
- grid draw
- pan / zoom
- node draw
- transition draw
- direct manipulation
- context menu
- floating editor
- validate badge overlay

### 8.2 Outliner Panel

window title:

```cpp
ICON_FA_LIST " GameLoop Outliner##GLE"
```

Outliner は補助である。

表示:

- search
- nodes
- transitions
- validate badges

操作:

- select
- frame selected
- rename via context

禁止:

- Outliner に Add Node / Add Transition の大きな button を常設しない
- Outliner でしかできない操作を作らない

### 8.3 Properties Panel

window title:

```cpp
ICON_FA_SLIDERS " GameLoop Properties##GLE"
```

Properties は selection context によって表示を切り替える。

Node selected:

- name
- scenePath
- start node toggle
- scene validate status
- duplicate / delete

Transition selected:

- name
- from / to
- source-local priority
- condition mode
- loading policy
- duplicate / reverse / delete

Condition selected:

- condition type
- actionIndex / actorType / threshold / seconds / targetName

None:

- short help
- selected asset summary

### 8.4 Validate Panel

window title:

```cpp
ICON_FA_TRIANGLE_EXCLAMATION " GameLoop Validate##GLE"
```

役割:

- validate summary
- error / warning list
- click message to select related object

Validate panel は bottom dock の tab とする。

### 8.5 Runtime Panel

window title:

```cpp
ICON_FA_GAUGE_HIGH " GameLoop Runtime##GLE"
```

役割:

- current node
- previous node
- pending node
- current scene
- pending scene
- waiting load
- node timer

Runtime panel は Validate と同じ bottom dock に tab でよい。

## 9. Graph Canvas 操作

### 9.1 Pan

必須:

- right drag: pan
- middle drag: pan
- Space + left drag: pan

right click と right drag は区別する。

PlayerEditor と同じように、right press point を持ち、移動量が 4px 未満なら context menu、4px 以上なら pan とする。

### 9.2 Zoom

mouse wheel zoom。

必須:

- mouse cursor を中心に zoom する
- min zoom: 0.25
- max zoom: 3.0
- Zoom 100%
- Fit Graph

### 9.3 Fit Graph

PlayerEditor の `FitGraphToContent` と同じ方式を使う。

対象:

- node rect
- transition label rect は Phase 2 で考慮

padding:

```cpp
constexpr float kFitPadding = 80.0f;
```

初期表示、New Default Loop、Load、Apply Preset 後は必ず fit request を立てる。

### 9.4 Grid

Graph panel に grid を表示する。

- zoom に応じて grid step を変える
- grid step が小さすぎる場合は非表示
- major grid は Phase 2

## 10. Node 表示仕様

現在の node は横幅が狭く、scenePath と transition label が重なっている。

新 node size:

```cpp
constexpr float kNodeWidth = 260.0f;
constexpr float kNodeHeight = 92.0f;
```

Node layout:

```text
+--------------------------------+
| Title                    [OK]  |
| Data/Scenes/Title.scene        |
| START              out-count 2 |
+--------------------------------+
```

Text は clip する。

禁止:

- scenePath を node 幅からはみ出して描画する
- node title と badge を重ねる
- zoom 0.5 未満で細かい文字を無理に表示する

### 10.1 Node Drag

node body drag で移動できること。

移動中:

- transition は追従
- graphPos は `delta / zoom` で更新
- selected node border を強調

### 10.2 Node Context

node right click:

- Rename
- Edit Scene Path
- Set As Start
- Duplicate
- Connect From Here
- Delete
- Frame Selected

### 10.3 Node Inline Edit

node title double click:

- inline rename

scenePath double click:

- inline scenePath edit
- `NormalizeGameLoopScenePath` を通す

## 11. Transition 表示仕様

現在の transition label と condition chip は重なっており、node も潰している。

改善:

- transition label は edge midpoint から少し上に offset
- condition chips は label の下に出すが、node rect と重なる場合は非表示または compact 表示
- zoom が低い場合は condition chip を非表示にして count badge のみにする
- loading badge は label 右端に置く

表示例:

```text
#0 Confirm [Fade]
[Input Confirm]
```

zoom < 0.65:

```text
#0 Confirm  C:1
```

### 11.1 Transition Hit Test

矢印選択は label だけに依存しない。

必須 hit target:

- line / bezier 近傍
- arrow head
- label rect

PlayerEditor の midpoint hit より広くする。

### 11.2 Transition Context

transition right click:

- Edit
- Add Condition
- Priority Up
- Priority Down
- Duplicate
- Reverse
- Set Loading
- Delete

### 11.3 Floating Transition Editor

arrow double click:

- floating editor を arrow 近くに出す

ただし floating editor は graph を覆いすぎない。

最大サイズ:

```cpp
width  = 420
height = min(content, 520)
```

condition editor は scroll child に入れる。

## 12. Condition 表示仕様

condition chip は graph を汚しすぎない。

ルール:

- zoom < 0.65 では chip 非表示
- chip は最大 2 件まで表示
- 3 件以上は `+N` badge
- chip は transition label と 4px 以上離す
- chip は node rect と重なるなら省略

Properties / floating editor では全 condition を編集できる。

## 13. Layout / Text 崩れ対策

必須:

- node text clip rect
- transition label max width
- condition chip max width
- horizontal overflow しない
- panel width が狭い場合、toolbar menu に逃がす
- bottom inspector を固定高で置かない

禁止:

- `Current: path` が toolbar button を押し出す
- long scenePath をそのまま全文表示して node を破壊する
- condition chip を無限に横へ並べる
- graph node を初期状態で中央に重ねる

## 14. Initial Asset Auto Layout

Default loop の node position は重ならない値にする。

推奨:

```text
Title   (0,   0)
Battle  (360, 0)
Result  (720, 0)
```

Result -> Battle の retry transition は下側 arc として描画する。

Result -> Title は上側 arc として描画する。

同じ node pair / reverse pair がある場合:

- bezier offset を変える
- label offset を変える

## 15. Panel 移動 / Dock 操作

GameLoopEditor は PlayerEditor と同じく dockable sub-window にする。

要件:

- Graph / Outliner / Properties / Validate / Runtime を個別に dock / undock できる
- Rebuild Layout で初期配置に戻せる
- root window は `NoScrollbar | NoScrollWithMouse`
- Graph panel は padding 0

## 16. Empty State

asset に node がない場合、graph 中央に最小限の action を表示する。

```text
Create Default Loop
Add Title Node
```

この button は empty state のみ表示。

通常時に graph 上へ常設しない。

## 17. Selection Context

GameLoopEditor に `SelectionContext` を導入する。

```cpp
enum class SelectionContext
{
    None,
    Node,
    Transition,
    Condition,
    ValidateMessage,
    Runtime,
};
```

選択更新:

- node click: Node
- transition click: Transition
- condition chip click: Condition
- validate message click: ValidateMessage
- background click: None

Properties panel は `SelectionContext` だけを見る。

## 18. 実装フェーズ

### Phase 0: 画面崩壊の修正

目的:

初期画面をまともにする。

実装:

- DockSpace 化
- Graph / Outliner / Properties / Validate / Runtime 分離
- bottom fixed inspector 削除
- text button toolbar を File/Create/View menu に整理
- graph padding 0
- initial fit graph
- default node positions 修正
- transition label / condition chip overlap 回避

完成条件:

- 初期画面で node が重ならない
- transition label が node と重ならない
- panel が dock / move できる
- graph canvas が主領域になる
- toolbar が 1 行に収まる

### Phase 1: Graph 直接操作の成立

実装:

- node drag
- right drag pan
- wheel zoom
- Fit Graph
- background right click add node
- node right click menu
- transition right click menu
- arrow hit test
- arrow double click floating editor
- output pin drag transition 作成

完成条件:

- Outliner / Properties を使わずに node を配置し transition を作れる
- node を自由に動かせる
- transition を直接選択 / 削除 / reverse / priority 変更できる

### Phase 2: 編集品質

実装:

- transition route offset
- label collision avoidance
- condition chip compact mode
- selected object frame
- drag drop scene file
- validate message click to focus
- unsaved dirty indicator

完成条件:

- 3 node / 4 transition の default loop が読みやすい
- 10 node / 20 transition でも最低限破綻しない

### Phase 3: Production

実装:

- undo / redo
- copy / paste
- multi select
- group move
- auto layout
- minimap

## 19. 受け入れ条件

必須:

- GameLoopEditor 初期画面で graph が中央主領域として表示される
- node が初期状態で重ならない
- transition label / condition chip が初期状態で重ならない
- panel を dock / undock / move できる
- Rebuild Layout で初期 dock layout に戻せる
- Toolbar の text button 横並びをやめ、File/Create/View menu になる
- bottom 固定 inspector が消える
- Properties は右 dock panel になる
- Validate / Runtime は bottom dock tab になる
- node body drag で node を動かせる
- right drag / middle drag で graph を pan できる
- mouse wheel で zoom できる
- Fit Graph で全 node が見える
- background right click で node を作れる
- node right click で rename / edit path / set start / duplicate / delete ができる
- output pin drag で transition を作れる
- arrow を直接クリックして transition を選択できる
- arrow right click menu が出る
- arrow double click で floating transition editor が出る
- condition chip は zoom / overlap に応じて compact 表示になる
- Debug x64 build が通る

## 20. 禁止事項

禁止:

- GameLoopEditor を単一 `BeginChild` 詰め込み UI に戻すこと
- bottom に巨大 inspector を固定すること
- Add Node / Add Transition button を常時 outliner に置くこと
- transition selection を label だけに依存すること
- node / transition text を clip せず描画すること
- 初期表示で node が重なること
- 初期表示で transition label が node を潰すこと
- toolbar に大量の text button を横並びにすること
- Graph panel より Inspector / Outliner が目立つ layout にすること
- `collapsed` / `color` を Phase 0 / Phase 1 の主作業に混ぜること

## 21. 最初に直す順序

最優先:

1. DockSpace 化
2. sub panel 分離
3. toolbar menu 化
4. initial fit graph
5. default node position 修正
6. transition label / chip overlap 回避
7. node drag / graph pan / zoom
8. arrow direct hit / context menu

この順序を守る。

今の問題は細かい機能不足ではなく、画面構造そのものが壊れているため、まず layout を直す。
