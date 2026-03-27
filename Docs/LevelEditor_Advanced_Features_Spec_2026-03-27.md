# レベルエディター拡張仕様書 2026-03-27

## 目的

現在の ECS ベース level editor を、Unity / Unreal Engine に近い日常運用レベルまで引き上げる。  
本仕様は既存の
- Hierarchy / Inspector / Asset Browser
- ECS Undo / Redo
- Prefab
- ImGuizmo 操作
- Scene save / load

を前提に、その次に入れるべき editor 機能を整理する。

---

## 到達目標

- scene 上での配置・選択・編集が直感的で速い
- Hierarchy / Inspector / Scene View の往復回数を減らす
- prefab / scene / asset の編集導線が自然につながる
- UE / Unity 的な基本操作を一通り満たす
- UI が「どこで何をすればよいか」を視覚的に示し、初見でも迷いにくい

---

## UI 設計原則

- Scene View / Hierarchy / Inspector / Asset Browser の責務を混ぜない
- create 系は Hierarchy に寄せ、Scene View は選択・変形・配置・視点操作に寄せる
- destructive action には確認ダイアログを出す
- 頻出操作は toolbar / context menu / shortcut の三系統で到達可能にする
- icon は「意味が一目で分かる操作」にだけ付け、全ボタンに乱用しない

### icon 利用ルール
- save: `ICON_FA_FLOPPY_DISK`
- open scene: `ICON_FA_FOLDER_OPEN`
- new scene: `ICON_FA_FILE`
- move / rotate / scale: 既存の `W / E / R` を主表示にし、必要なら補助 icon を併記
- visible: `ICON_FA_EYE`
- hidden: `ICON_FA_EYE_SLASH`
- active / inactive: `ICON_FA_POWER_OFF`
- prefab: `ICON_FA_CUBE`
- add component: `ICON_FA_PLUS`
- search: `ICON_FA_MAGNIFYING_GLASS`
- bookmark / focus 系は icon を付けてもよいが、初期版は文字優先

### icon を使うべきタイミング
- toolbar の主操作
- Hierarchy の状態表示
- Asset Browser のファイル種別表示
- context menu の create / save / load / delete

### icon を使いすぎない箇所
- Inspector の詳細プロパティ行
- 長文確認ダイアログ
- 数値入力欄

---

## 優先度

### S 優先
1. 複数選択
2. gizmo snap
3. asset drag & drop による scene 配置
4. Alt + Drag duplicate
5. Scene View 上での右クリック選択メニュー改善
6. camera speed / camera orbit 改善

### A 優先
1. local / world の視覚表示強化
2. entity の active / visible 切替
3. hierarchy 検索 / filter
4. prefab instance の apply / revert 導線強化
5. scene dirty 状態表示
6. autosave / recovery

### B 優先
1. box select / marquee select
2. pivot / center 切替
3. vertex / surface snap
4. duplicate special
5. scene bookmark camera
6. grouping / folder entity

---

## Phase 1: 選択と変形の強化

### 1-1. 複数選択
- Hierarchy で Ctrl 追加選択、Shift 範囲選択を可能にする
- Scene View クリック選択も Ctrl 追加選択対応
- EditorSelection は単一 entity ではなく複数 entity 集合を保持できる形へ拡張する
- 選択集合の中に primary entity を 1 つ持つ
- primary entity は Inspector の基準、Focus 対象、prefab 操作対象、gizmo の基準 entity とする
- Inspector は初期版では
  - 共通 component のみ表示
  - Transform の一括編集のみ対応
- gizmo の基準点は初期版では選択群の中心

### 1-2. Gizmo snap
- 移動 snap
- 回転 snap
- scale snap
を追加する
- ツールバーに snap ON/OFF と step 値を持つ
- 初期値例
  - move: 0.5
  - rotate: 15
  - scale: 0.1
- gizmo drag 中の snap は ImGuizmo 出力後の transform 値へ適用する
- snap 状態は toolbar に icon 付きで表示する
  - snap on/off: `ICON_FA_MAGNET`

### 1-3. Alt + Drag duplicate
- gizmo drag 開始時に Alt が押されていた場合のみ duplicate を 1 回作成してから移動する
- drag 途中で Alt を押しても duplicate は発生させない
- mouse down から mouse up までを 1 transaction とする
- duplicate 後の選択対象は新規 entity 群へ切り替える
- prefab instance でも duplicate は許可。ただし child override 制限は既存仕様に従う

### 1-4. Pivot / Center 切替
- gizmo 原点を
  - pivot
  - selection center
で切り替える
- 初期版では toolbar toggle のみ

---

## Phase 2: Scene View 配置導線の強化

### 2-1. Asset Browser から Scene へ drag & drop 配置
- model asset を Scene View へドラッグしたら entity を生成して配置する
- prefab asset を Scene View へドラッグしたら instance 化して配置する
- material asset は mesh 選択中に drop した場合のみ assign 候補にする
- 配置位置は以下の順で決める
  1. PhysicsManager::CastRay に当たる surface
  2. 地面 plane
  3. camera forward 方向の固定距離
- drag 開始から配置確定までを 1 transaction とする

### 2-2. Scene View 右クリックメニュー改善
- create 系は Hierarchy に限定済みなので、Scene View 右クリックは「選択 / 配置 / view 操作」中心にする
- 初期 menu
  - Focus
  - Frame Selected
  - Duplicate Here
  - Delete
  - Reset Camera
- 将来的に context-sensitive menu を追加できる構造にする
- destructive な項目には icon と確認導線を付ける

### 2-3. Drag placement preview
- drag 中は ghost 表示を出す
- 配置確定前に
  - hit position
  - normal
  - rotation candidate
を計算する
- Esc でキャンセル可能

---

## Phase 3: Camera 操作の強化

### 3-1. camera speed
- Scene View camera speed を toolbar から変更可能にする
- Shift で加速、Ctrl で微速移動
- speed は editor setting に保存する
- toolbar 上は compact 表示を優先する

### 3-2. Orbit / Pan / Fly の明文化
- RMB: fly
- MMB: pan
- Alt + LMB: orbit target
- F 後はその target を orbit 基準にする

### 3-3. Camera bookmark
- 1〜9 の bookmark を保存 / 復元できるようにする
- scene file には保存せず editor local setting 扱いでもよい

---

## Phase 4: Hierarchy / Inspector の強化

### 4-1. Hierarchy 検索
- 名前検索
- component 種別 filter
- active only / prefab only filter
- 検索欄には `ICON_FA_MAGNIFYING_GLASS` を付ける

### 4-2. active / visible 切替
- entity 単位で active フラグを持てるようにする
- visible は editor render / picking 用、active は gameplay / update 用の責務として分離する
- Hierarchy の目アイコン / 有効無効アイコンを追加する
- 初期版では以下を保証する
  - visible false: render 対象外 / picking 対象外
  - active false: 少なくとも editor 表示は維持可能、runtime update との連動は段階導入

### 4-3. Inspector 複数編集
- 共通 component のみ表示
- Transform の数値一括変更
- bool / enum の一括変更
- mixed value 表示は将来対応でもよい

### 4-4. component add menu 強化
- Add Component のカテゴリ分け
- 検索ボックス
- 最近使った component
- `Add Component` ボタンには `ICON_FA_PLUS` を付ける

---

## Phase 5: Scene / Prefab ワークフロー強化

### 5-1. Scene dirty 状態表示
- 未保存変更がある時は window title または toolbar に `*` を出す
- dirty にする操作
  - transform 変更
  - create / delete / duplicate
  - reparent
  - component edit
  - prefab apply / revert
- scene load / save / new scene 後は dirty を clear
- save button には `ICON_FA_FLOPPY_DISK` を付ける

### 5-2. Save before destructive action
- dirty scene 状態で
  - New Scene
  - Open Scene
  - scene file double click load
時は確認ダイアログを出す
- 選択肢
  - Save
  - Don't Save
  - Cancel
- destructive popup では文字を優先し、icon は補助程度にする

### 5-3. Autosave / Recovery
- 一定間隔で autosave scene を保存
- 起動時に recovery scene があれば復元確認を出す
- autosave は `Saved/Autosave/*.scene` に置く
- autosave は scene 名ごとに複数世代保持する
- 起動時の recovery 判定は基本的に各 scene の最新 autosave を見る

### 5-4. Prefab instance 表示強化
- Hierarchy 上で prefab instance icon を表示
- Inspector に source prefab path を表示
- Apply / Revert / Unpack を上段固定表示
- child 構造 override 未対応の制限は明示する
- prefab 関連操作には `ICON_FA_CUBE` を使う

---

## Phase 6: Editor UX の仕上げ

### 6-1. Toolbar の整理
- save
- snap toggle
- move / rotate / scale
- local / world
- pivot / center
- camera speed
を一列または二段で整理する
- 主要操作には icon を付ける
- 文字だけのボタンと icon ボタンを混ぜる場合は、頻度の高い操作を icon 優先にする

### 6-2. 見た目の統一
- button サイズ
- spacing
- selected color
- disabled color
- popup width
を統一する
- toolbar / context menu / modal ごとに padding ルールを決める

### 6-3. Status bar
- scene 名
- dirty 状態
- selection count
- snap 値
- camera speed
- play/edit mode
を表示する

### 6-4. File menu / toolbar の責務
- File menu には
  - `ICON_FA_FILE` New Scene
  - `ICON_FA_FOLDER_OPEN` Open Scene
  - `ICON_FA_FLOPPY_DISK` Save
  - Save As
  を置く
- toolbar には Save を置くが、Open / New は menu 優先でもよい

---

## データ設計方針

### EditorSelection
- 単一 entity から複数 entity 集合へ拡張する
- 選択集合とは別に primary entity を保持する
- ただし asset selection と entity selection は排他的に維持する

### Undo / Redo
- multi-select edit は composite command
- Alt + Drag duplicate は 1 transaction
- drag placement も 1 transaction
- scene load / new scene は undo 対象外、履歴 clear

### Physics picking
- Scene View 上の配置 / 選択 / drag placement は PhysicsManager を基準にする
- collider がない可視 entity は editor fallback を残してよい

### Scene dirty
- Undo / Redo 実行でも dirty 状態を再評価する
- 将来的には command stack index と last saved index を保持する

---

## 非目標
- runtime gameplay editor
- prefab child 構造 override 完全対応
- terrain editor
- material graph editor
- animation editor
- sequencer

---

## 実装順の推奨
1. 複数選択
2. gizmo snap
3. Alt + Drag duplicate
4. asset drag & drop scene placement
5. dirty scene 表示 + save before destructive action
6. camera speed / orbit 改善
7. hierarchy 検索 / active toggle
8. autosave / recovery

---

## 完了条件
- Unity / UE と同じ感覚で、scene 上で
  - 選択
  - 変形
  - 複製
  - 配置
  - 保存
  が自然にできる
- 新規ユーザーが Hierarchy と Scene View だけで簡単なレベル編集を完了できる
- Undo / Redo と scene / prefab ワークフローが破綻しない
- build 成功、window 起動、runtime error 0 を維持する
