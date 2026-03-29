# レベルエディター Edit / View / Window メニュー仕様書

作成日: 2026-03-29

## 目的
- 既存のレベルエディターに `Edit` `View` `Window` メニューを実装し、Unity / Unreal Engine に近い基本操作導線を整える
- 既存ショートカット、ツールバー、右クリックメニュー、パネル表示フラグを **同じ command** に統一する
- ファイル操作、編集操作、表示切替、パネル管理の責務を明確に分離し、初心者でも迷いにくく、上級者でも触りやすい UI にする

## 責務分離
- `File` = scene / project ファイル操作
- `Edit` = 編集 command
- `View` = viewport / overlay / authoring 表示の切替
- `Window` = パネル開閉、レイアウト、ツールウィンドウ管理

## 参考方針
### Unity 寄り
- `Edit` に Undo / Redo / Duplicate / Delete / Select All / Copy / Paste を置く
- `View` に 2D/3D、gizmo、grid、safe area、stats、overlay 切替を置く
- `Window` で Hierarchy / Scene / Game / Inspector / Console / Project 相当を開く

### Unreal Engine 寄り
- `View` に viewport 表示補助と camera 操作補助を置く
- `Window` に layout 復旧と tool window 管理を置く
- すべての UI 操作がメニューから辿れる状態にする

## 対象
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Layer\EditorLayer.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Layer\EditorLayer.h`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Hierarchy\HierarchyECSUI.cpp`
- `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload\Source\Inspector\InspectorECSUI.cpp`
- 既存 dockspace / panel 表示フラグ管理

## 完成イメージ
- `File` で scene を扱う
- `Edit` で編集 command を扱う
- `View` で見え方と authoring 補助を切り替える
- `Window` で窓を開閉・復旧・フォーカスする
- ショートカット、ツールバー、コンテキストメニュー、メニューバーがすべて同じ command を使う

---

## 1. Edit メニュー

## 1-1. 目的
- 現在の editor 操作 command をメニューからも辿れるようにする
- 既存 shortcut-only 操作を discoverable にする
- `Edit` は **command のカタログ** として振る舞う

## 1-2. 初期版必須項目
1. `Undo`
2. `Redo`
3. `Duplicate`
4. `Delete`
5. `Frame Selected`
6. `Select All`
7. `Deselect`
8. `Rename`
9. `Copy`
10. `Paste`

## 1-3. 拡張項目
11. `Reset Transform`
12. `Create Empty Parent`
13. `Unpack Prefab`
14. `Focus Search`

## 1-4. 項目詳細
### `Undo`
- ショートカット: `Ctrl+Z`
- `UndoSystem::Undo(registry)` を呼ぶ
- Undo 不可時は disabled

### `Redo`
- ショートカット:
  - `Ctrl+Y`
  - `Ctrl+Shift+Z`
- `UndoSystem::Redo(registry)` を呼ぶ
- Redo 不可時は disabled

### `Duplicate`
- ショートカット: `Ctrl+D`
- 現在選択中の root entity 群を複製
- 複数選択時も 1 command / 1 undo 単位
- 選択がない場合は disabled

### `Delete`
- ショートカット: `Delete`
- 現在選択中の root entity 群を削除
- prefab 制約は既存ルールに従う
- 選択がない場合は disabled

### `Frame Selected`
- ショートカット: `F`
- 3D mode: primary selection を camera focus
- 2D mode: primary selection の `RectTransformComponent` を対象に center / zoom 調整
- primary selection が無ければ disabled

### `Select All`
- ショートカット: `Ctrl+A`
- Hierarchy に表示されている編集対象 entity を全選択
- utility entity は除外
  - `Environment`
  - `Reflection Probe`
  - 将来の hidden editor-only utility

### `Deselect`
- ショートカット: `Esc`
- 選択集合を空にする

### `Rename`
- ショートカット: `F2`
- primary selection の名前編集へ入る
- 初期版では Hierarchy 行 rename UI へフォーカスを移せればよい

### `Copy`
- ショートカット: `Ctrl+C`
- 現在選択中 root entity 群を editor clipboard に snapshot 保存
- OS クリップボードではなく editor 内 clipboard

### `Paste`
- ショートカット: `Ctrl+V`
- editor clipboard から複製して scene に貼り付け
- 親無しで貼る場合は root 配置
- 選択中 entity が parent 化可能なら子として貼り付けてもよい

### `Reset Transform`
- 対象: primary selection
- 3D: position=(0,0,0), rotation=identity, scale=(1,1,1)
- 2D: `RectTransformComponent` の anchoredPosition / rotation / size / scale を既定値へ
- 1 command / 1 undo

### `Create Empty Parent`
- primary selection か root selection 群を新規 parent の子に束ねる
- Unity の `Create Empty Parent` 相当
- 初期版では単一 selection 対応でも可

### `Unpack Prefab`
- prefab root 選択時のみ有効
- 既存 unpack command をメニュー化

### `Focus Search`
- ショートカット: `Ctrl+K`
- Hierarchy 検索欄へフォーカス

## 1-5. command 統一ルール
- `Edit` メニューから呼ぶ command は以下と同一実装を使う
  - toolbar
  - keyboard shortcut
  - hierarchy context menu
  - scene view 操作
- `Edit` 専用の別処理は作らない

## 1-6. enabled / disabled
- `Undo`: undo stack が空なら disabled
- `Redo`: redo stack が空なら disabled
- `Duplicate`: selection が空なら disabled
- `Delete`: selection が空なら disabled
- `Frame Selected`: primary selection が無ければ disabled
- `Select All`: 対象 entity が無ければ disabled
- `Deselect`: selection が空なら disabled
- `Rename`: primary selection が無ければ disabled
- `Copy`: selection が空なら disabled
- `Paste`: editor clipboard が空なら disabled
- `Reset Transform`: primary selection が無ければ disabled
- `Create Empty Parent`: selection が空なら disabled
- `Unpack Prefab`: prefab root 選択時のみ enabled

## 1-7. 表示順
- `Undo / Redo`
- separator
- `Copy / Paste / Duplicate / Delete / Rename`
- separator
- `Frame Selected / Select All / Deselect / Focus Search`
- separator
- `Reset Transform / Create Empty Parent / Unpack Prefab`

---

## 2. View メニュー

## 2-1. 目的
- viewport の見え方と補助表示を切り替える
- `Scene View` と `Game View` の表示系 command をメニューから辿れるようにする
- 2D/3D authoring の mode 切替もここへ集約する

## 2-2. 初期版必須項目
1. `2D Mode`
2. `3D Mode`
3. `Show Grid`
4. `Show Gizmo`
5. `Show Safe Area`
6. `Show Pixel Preview`
7. `Show Stats Overlay`
8. `Show Selection Outline`
9. `Reset View`
10. `Focus Scene View`
11. `Focus Game View`

## 2-3. 拡張項目
12. `Shading Mode > Lit / Unlit / Wireframe`
13. `Show Light Icons`
14. `Show Camera Icons`
15. `Show Bounds`
16. `Show Collision`
17. `Snap > Translate / Rotate / Scale`
18. `Camera Speed > preset`
19. `Bookmarks > Save / Recall`

## 2-4. 項目詳細
### `2D Mode`
- `Scene View` を 2D authoring mode に切り替える
- 既存 `SceneViewMode::Mode2D` を使う
- check mark 付き

### `3D Mode`
- `Scene View` を 3D authoring mode に切り替える
- 既存 `SceneViewMode::Mode3D` を使う
- `2D Mode` と排他的

### `Show Grid`
- editor grid overlay の on/off
- `Scene View` にのみ効く

### `Show Gizmo`
- gizmo の表示 on/off
- ImGuizmo 自体は維持しつつ、描画と入力受付を切り替える

### `Show Safe Area`
- `Game View` の safe area overlay on/off
- 現在の `m_gameViewShowSafeArea` を正にする
- check mark 付き

### `Show Pixel Preview`
- `Game View` の pixel preview overlay on/off
- 現在の `m_gameViewShowPixelPreview` を正にする
- check mark 付き

### `Show Stats Overlay`
- FPS、frame time、scene stats、draw stats の overlay 表示 on/off
- 初期版では scene/game view 内の簡易テキスト overlay でよい

### `Show Selection Outline`
- primary selection の表示補助 on/off
- 初期版では gizmo/枠線/2D rect outline の切替でもよい

### `Reset View`
- `Scene View` の camera / zoom / pan / focus 状態を既定に戻す
- 3D mode では editor camera を既定位置へ
- 2D mode では center = (0,0), zoom = default

### `Focus Scene View`
- `Scene View` にフォーカス移動
- 次 frame に対象 window を前面化 / keyboard focus を与える

### `Focus Game View`
- `Game View` にフォーカス移動

### `Shading Mode`
- 初期版は仕様のみ先行可
- `Lit`
- `Unlit`
- `Wireframe`
- 将来の render debug への入口

### `Snap`
- `Translate Snap` on/off
- `Rotate Snap` on/off
- `Scale Snap` on/off
- 既存 Scene View toolbar の状態と統一

### `Camera Speed`
- `Slow / Normal / Fast / Very Fast`
- 数値 UI と共存してよい

### `Bookmarks`
- authoring 用 camera view を保存/復帰
- 初期版は 3 スロット程度でよい

## 2-5. View と Window の線引き
- `View` は **同じ窓の中の見え方** を切り替える
- `Window` は **窓そのものの表示/非表示** を切り替える

例:
- safe area → `View`
- Scene View を閉じる → `Window`
- 2D/3D mode → `View`
- Lighting Settings を開く → `Window`

## 2-6. enabled / disabled
- `2D Mode` / `3D Mode`: 常時使用可
- `Show Safe Area` / `Show Pixel Preview`: `Game View` が閉じていても設定変更可
- `Show Gizmo`: selection 無しでも切替可
- `Reset View`: 常時使用可
- 拡張項目は未接続なら disabled で先出し可

---

## 3. Window メニュー

## 3-1. 目的
- 現在開いている editor panel を再表示できるようにする
- 見失ったパネルをいつでも戻せるようにする
- Unity の `Window` / UE の `Window` 相当の役割を持たせる

## 3-2. 初期版必須項目
1. `Scene View`
2. `Game View`
3. `Hierarchy`
4. `Inspector`
5. `Asset Browser`
6. `Console`
7. `Lighting Settings`
8. `G-Buffer Debug`
9. `Reset Layout`
10. `Focus Hierarchy`
11. `Focus Inspector`
12. `Focus Asset Browser`
13. `Focus Console`

## 3-3. 拡張項目
14. `Maximize Active Panel`
15. `Restore Panel Layout`
16. `Close All Secondary Windows`
17. `Show Status Bar`
18. `Show Main Toolbar`
19. `Panels > ...` サブメニュー化
20. `Tools > ...` サブメニュー化

## 3-4. パネル表示ルール
- 各項目は bool フラグで開閉状態を持つ
- `Window` メニュー内では check mark 付きメニューとして出す
- `false -> true` にしたときは次 frame で表示
- panel を閉じたときもフラグへ状態を戻す

## 3-5. 各項目の意味
### `Scene View`
- authoring 用 view
- selection / gizmo / placement / 2D authoring の主戦場

### `Game View`
- runtime preview view
- resolution preview / safe area / UI preview の確認窓

### `Hierarchy`
- ECS scene の tree 表示

### `Inspector`
- primary selection を編集

### `Asset Browser`
- asset 操作
- prefab / scene / sprite / font の入口

### `Console`
- log 表示

### `Lighting Settings`
- `EnvironmentComponent`
- `ReflectionProbeComponent`
- `PostEffect`
を編集する補助ウィンドウ

### `G-Buffer Debug`
- render debug 用ウィンドウ

### `Reset Layout`
- dock layout を初期状態へ戻す
- パネルが見失われたときの復旧手段
- Unity/UE のレイアウト復帰に相当

### `Focus ...`
- 対象 panel を前面にし、keyboard focus も寄せる
- 複数 panel を行き来しやすくする

### `Maximize Active Panel`
- active な panel を一時最大化
- 再実行で元に戻るのが理想

### `Close All Secondary Windows`
- `Lighting Settings` や `G-Buffer Debug` など補助ウィンドウを一括で閉じる
- main panels は閉じない

## 3-6. utility entity 非表示との関係
- `Environment` と `Reflection Probe` は Hierarchy に出さない
- 代わりに `Window -> Lighting Settings` から編集する
- これは Unity の Lighting Settings に近い UX を狙う

---

## 4. UI 仕様

## 4-1. メニュー表記
- `Edit(E)`
- `View(V)`
- `Window(W)`

## 4-2. アイコン方針
メニューバー直下のメニュー項目は文字中心とし、過度な icon は入れない。  
ただし補助的に先頭 icon を使うのは可。

推奨:
- `Undo` → rotate-left
- `Redo` → rotate-right
- `Duplicate` → copy
- `Delete` → trash
- `Frame Selected` → crosshair / expand
- `Rename` → i-cursor
- `Copy` → copy
- `Paste` → clipboard
- `2D Mode` → layer-group / vector-square
- `3D Mode` → cube
- `Show Grid` → border-all
- `Show Gizmo` → arrows-up-down-left-right
- `Show Safe Area` → mobile-screen
- `Show Pixel Preview` → table-cells-large
- `Show Stats Overlay` → chart-line
- `Hierarchy` → list
- `Inspector` → sliders
- `Asset Browser` → folder-open
- `Console` → terminal
- `Lighting Settings` → sun
- `G-Buffer Debug` → images
- `Reset Layout` → window-restore
- `Focus ...` → up-right-from-square / bullseye

## 4-3. check mark
- `View` と `Window` の toggle 項目は check mark 付き
- dialog 的な一時ウィンドウも、表示フラグを持つものは check mark 対応

## 4-4. disabled 表示
- `Edit` の disabled 項目はグレーアウト
- 未実装の `View` / `Window` 項目を先出しする場合も disabled を使う
- 押しても何も起きない silent fail は避ける

## 4-5. サブメニュー構成
- 項目が増えてきたら以下へ整理する
  - `View > Overlays`
  - `View > Camera`
  - `View > Shading`
  - `Window > Panels`
  - `Window > Tools`
  - `Window > Layout`

---

## 5. 実装ルール

## 5-1. EditorLayer に持たせる状態
- `m_showSceneView`
- `m_showGameView`
- `m_showHierarchy`
- `m_showInspector`
- `m_showAssetBrowser`
- `m_showConsole`
- `m_showLightingWindow`
- `m_showGBufferDebug`
- `m_showStatusBar`
- `m_showMainToolbar`
- `m_showSceneGrid`
- `m_showSceneGizmo`
- `m_showSceneStatsOverlay`
- `m_showSceneSelectionOutline`
- `m_gameViewShowSafeArea`
- `m_gameViewShowPixelPreview`

既存の `Lighting` / `G-Buffer Debug` / `Game View` overlay フラグは流用し、残りを追加する。

## 5-2. RenderUI 側の分岐
- `RenderUI()` で各 panel の draw を呼ぶ前に show フラグを見る
- show false の panel は描かない
- toolbar / status bar も同様にフラグ管理する

## 5-3. 各 draw 関数の責務
- `DrawSceneView()`
- `DrawGameView()`
- `DrawHierarchy()`
- `DrawInspector()`
- `Console::Draw()`
- `AssetBrowser::RenderUI()`

それぞれはウィンドウ 1 個の責務に閉じる

## 5-4. Layout reset
- `Reset Layout` は dock builder を再生成する
- 初期レイアウトは以下
  - 左: Hierarchy
  - 中央: Scene View / Game View
  - 右: Inspector
  - 下: Console / Asset Browser
- `Reset Layout` は show フラグも既定値へ戻す

## 5-5. command 統一
- `Edit` / `View` / `Window` の command は、既存ショートカットと競合しないよう統一ヘルパーを通す
- 将来的には `EditorCommand` 的な薄い dispatcher に寄せてよい

## 5-6. primary selection の扱い
- `Edit` の多くの command は primary selection を基準にする
- 複数 selection がある場合も、Inspector / Frame / Rename / Reset Transform の基準は primary

---

## 6. 実装フェーズ

### Phase 1
- `Edit` に core command 接続
- enabled / disabled 制御

### Phase 2
- `Window` に core panel 開閉
- show フラグ導入

### Phase 3
- `View` に core mode / overlay / reset を追加
- `Scene View` / `Game View` 側のフラグと統合

### Phase 4
- `Reset Layout`
- `Focus ...` command
- utility entity の編集導線を Lighting 側へ集約

### Phase 5
- `Edit` 拡張項目 (`Rename`, `Copy`, `Paste`, `Reset Transform`, `Unpack Prefab`)
- `View` 拡張項目 (`Shading`, `Snap`, `Camera Speed`, `Bookmarks`)
- `Window` 拡張項目 (`Maximize`, `Close Secondary`, `Toolbar/StatusBar`)

### Phase 6
- polish
  - icon
  - check mark
  - disabled 見た目
  - フォーカス移動
  - メニュー整理

---

## 7. 完了条件
- `Edit` から Undo / Redo / Duplicate / Delete / Frame / Select All / Deselect / Rename / Copy / Paste が使える
- `View` から 2D/3D mode と overlay 切替、Reset View が使える
- `Window` から主要 panel を再表示できる
- `Reset Layout` で panel を復旧できる
- `Environment` と `Reflection Probe` は Hierarchy に出ず、Lighting から編集できる
- ショートカット / toolbar / context menu / menu bar が同じ command を使う

## 8. 非目標
- Preferences ウィンドウ
- package manager
- layout 保存スロット複数対応
- editor テーマ切替
- 完全な Maya/Blender クラスの viewport メニュー再現
