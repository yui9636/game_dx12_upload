# PlayerEditor 究極レイアウト 実装仕様書 v2.0
**日付**: 2026-05-18
**スローガン**: **「小学生でも触れるツール」**
**前提**: 既存機能 (StateMachine / Timeline / Skeleton / Sockets / Hitbox / Body / Input / Animator / Properties) を保持。新機能の追加は最小限。

**ラベル方針**: ImGui の ID 衝突回避 / フォント問題 / Docking 設定キー安定性のため、**全 UI ラベルは英語短語**で書く。代わりに**説明文・補足テキスト・冗長なヘルプ文を全カット**してわかりやすさを担保する。

**外観方針**: パネルは**不透明ダークテーマ**。半透明は採用しない（viewport の外側に置くパネルが透けても意味がないため）。代わりに**色階層・余白・タイポグラフィ**で洗練を出す。

---

## 0. 設計原則

1. **画面の 70% は viewport**。プレビュー領域を最大化。
2. **クリックする物は大きく、数を絞る**。アイコン主体、ラベルは英語短語。
3. **見たまま編集**。値スライダを動かしたらその瞬間にプレビューが変わる。
4. **テンプレは「Full Player」1 つだけ**。既存 `ApplyFullPlayerPreset` を流用、追加カードは作らない。
5. **Undo はレベルエディタと共有**。`UndoSystem::Instance()` 流用、PlayerEditor 専用履歴は作らない。
6. **余計な機能を足さない**。チュートリアル / オートセーブ / Undo サムネ / テンプレカード等は**全カット**。
7. **半透明は使わない**。色とコントラストで階層を作る。

---

## 1. 究極レイアウト

```
┌──────────────────────────────────────────────────────────────────┐
│  Top Bar (UE 風スタック・54px)                                   │
│  ┌────┐ ┃ ┌────┬────┐ ┃ ┌────┐ ┃ ┌────┬────┐ ┃ ┌────┬────────┐ │
│  │ ☰  │ ┃ │ 💾 │ 📂 │ ┃ │ ▼  │ ┃ │ ▶  │ ⏸  │ ┃ │ 🎯 │  ⚡    │ │
│  │Menu│ ┃ │Save│Open│ ┃ │Mode│ ┃ │Test│Edit│ ┃ │Fit │FullPlay│ │
│  └────┘ ┃ └────┴────┘ ┃ └────┘ ┃ └────┴────┘ ┃ └────┴────────┘ │
├──┬──────────────────────────────────────────────┬────────────────┤
│  │                                              │            │
│ Q│   ╔════════════════════════════════════╗     │            │
│ u│   ║                                    ║     │  Inspector │
│ i│   ║      3D Viewport (常時 70%+)       ║     │  (選択時   │
│ c│   ║      左上に EDIT バッジ            ║     │   のみ)    │
│ k│   ║                                    ║     │   320px    │
│  │   ╚════════════════════════════════════╝     │            │
│56│                                              │            │
│px│  ┌──────── Workbench (折畳可) ────────┐      │            │
│  │  │ Tab: Move Attack Hit Input Bone    │      │            │
│  │  │   選択タブの中身                   │      │            │
│  │  └────────────────────────────────────┘      │            │
├──┴──────────────────────────────────────────────┴────────────┤
│  Status (24px)                                               │
└──────────────────────────────────────────────────────────────┘
```

### 1.1 Top Bar（UE 風スタック・5 グループ）

**仕様**: 各ボタンは縦スタック構造 — 上にアイコン (22×22px)、下にラベル (10px・1 単語)。グループ間は薄い縦区切り線 `Border` 色で分離する。ただ並べるのではなく**意味の塊**ごとに区切る。

| グループ | ボタン | アイコン | ラベル | 機能 |
|---|---|---|---|---|
| G1 メニュー | 1 | `FA_BARS` | `Menu##tb` | プルダウン (New / Save As / Export / Exit) |
| G2 ファイル | 2 | `FA_FLOPPY_DISK` | `Save##tb` | `SavePrefabDocument(false)` (Ctrl+S 同等) |
|  | 3 | `FA_FOLDER_OPEN` | `Open##tb` | ファイルダイアログ |
| G3 種別 | 4 | `FA_CARET_DOWN` | `Mode##tb` | Combo: Player / Enemy / NPC |
| G4 モード | 5 | `FA_PLAY`   | `Test##tb` | テストモードへ (再生風アクセント色) |
|  | 6 | `FA_PEN`    | `Edit##tb` | 編集モードへ |
| G5 補助 | 7 | `FA_CROSSHAIRS` | `Fit##tb` | カメラフィット |
|  | 8 | `FA_BOLT`   | `Full Player##tb` | `ApplyFullPlayerPreset` |

**スタックボタン実装**:
- 1 ボタン = 64×46px のクリック領域、内側にアイコン (Y=4) + ラベル (Y=28)
- 選択中 (Edit/Test の現在モード) はアイコン色がシアン、背景が `ButtonActive`
- `Save` ボタンは dirty 時のみアイコンに小さい橙ドット (3px) を右上に表示
- グループ間は 8px の余白 + 1px の縦線

**削除**: `Setup Full Enemy/NPC` (Mode 切替時に内部処理)、`Reset Runtime` (モード切替で自動)、`Tool:` ラベル。
**Undo/Redo ボタン**: 出さない (Ctrl+Z/Y のみ、レベルエディタと履歴共有)。

### 1.2 Quick Tools（左 56px レール）
アイコン縦並び 5 個 + 折畳ボタン。

| アイコン | ラベル (ImGui ID) |
|---|---|
| `FA_PERSON_RUNNING` | `Move##qt` |
| `FA_BURST`          | `Attack##qt` |
| `FA_SHIELD`         | `Hit##qt` |
| `FA_GAMEPAD`        | `Input##qt` |
| `FA_BONE`           | `Bone##qt` |
| `FA_CHEVRON_DOWN`   | `Toggle##qt` |

ツールチップは無し。アイコンで判別。

### 1.3 Workbench（下 32%・単一 window）
- 1 つの ImGui window に TabBar、5 タブ。
- タブラベル: `Move` `Attack` `Hit` `Input` `Bone`
- 折畳時は高さ 0、上端タブだけ見える状態。

### 1.4 Inspector（右 320px・選択時のみ）
- 何も選択していないときは**描画しない**。
- 選択直後に右からスライドイン (200ms ease-out)。
- 内容は SelectionContext で 1 種のみ表示（既存 `DrawPropertiesPanel` の switch をそのまま流用）。

---

## 2. 不透明ダークテーマ（PlayerEditor 専用）

### 2.1 色階層（3 レベル）

| 階層 | 用途 | 色 (RGB) | α |
|---|---|---|---|
| L0 背景 | TopBar / Status | (0.045, 0.055, 0.075) | 1.0 |
| L1 パネル本体 | WindowBg | (0.060, 0.075, 0.100) | 1.0 |
| L2 入れ子 child | ChildBg | (0.080, 0.100, 0.130) | 1.0 |

3 段階の輝度差で階層を表現。境界線は薄シアン (0.42, 0.69, 0.88, 0.50) で 1px。

### 2.2 主要 ImGui Style 値
```cpp
WindowBg        (0.060, 0.075, 0.100, 1.0)
ChildBg         (0.080, 0.100, 0.130, 1.0)
PopupBg         (0.080, 0.100, 0.130, 1.0)
TitleBg         (0.045, 0.055, 0.075, 1.0)
TitleBgActive   (0.110, 0.150, 0.210, 1.0)   // アクティブタイトル: シアン寄り
MenuBarBg       (0.045, 0.055, 0.075, 1.0)
FrameBg         (0.110, 0.140, 0.180, 1.0)
FrameBgHovered  (0.150, 0.190, 0.240, 1.0)
FrameBgActive   (0.180, 0.230, 0.290, 1.0)
Tab             (0.060, 0.075, 0.100, 1.0)
TabHovered      (0.150, 0.210, 0.270, 1.0)
TabActive       (0.180, 0.260, 0.340, 1.0)   // アクティブタブ: 明シアン
TabUnfocused    (0.055, 0.070, 0.092, 1.0)
TabUnfocusedActive (0.110, 0.150, 0.200, 1.0)
Border          (0.420, 0.690, 0.880, 0.50)
Separator       (0.420, 0.690, 0.880, 0.32)
Header          (0.110, 0.150, 0.200, 1.0)
HeaderHovered   (0.150, 0.200, 0.260, 1.0)
HeaderActive    (0.180, 0.260, 0.340, 1.0)
Button          (0.110, 0.150, 0.200, 1.0)
ButtonHovered   (0.150, 0.210, 0.275, 1.0)
ButtonActive    (0.180, 0.270, 0.350, 1.0)   // アクセント
Text            (0.92, 0.94, 0.97, 1.0)
TextDisabled    (0.50, 0.55, 0.62, 1.0)
```

### 2.3 サイズ・余白
```cpp
WindowRounding  = 6.0f
ChildRounding   = 4.0f
FrameRounding   = 3.0f
TabRounding     = 5.0f
WindowPadding   = (10, 8)
ItemSpacing     = (8, 6)
WindowBorderSize = 1.0f
FrameBorderSize  = 0.0f   // フレーム枠は色差で十分
```

### 2.4 装飾
- アクティブタブの下端に 2px のシアンアクセント線を `DrawList::AddLine` で重ねる
- Workbench window の上端に 1px の薄ハイライト線 `(0.55, 0.85, 1.0, 0.25)`
- 影は使わない（不透明前提なので不要）

---

## 3. わかりやすさ仕様

| 項目 | 仕様 |
|---|---|
| **ラベル** | 英語短語 (`Move` `Attack` `Hit` `Input` `Bone`)。1〜2 単語、ImGui ID 重複回避のため `##suffix` を必ず付与 |
| **説明文** | パネル内の解説テキスト・hint・helper コメント全カット。アイコンとレイアウトで意図を伝える |
| **フォント** | 通常 16px、ボタン 18px、見出し 20px |
| **クリック範囲** | 32×32px 以上 |
| **ツールチップ** | 出さない。アイコン形状で識別 |
| **ドラッグ&ドロップ** | 骨 → コライダ一覧 / タイムライン項目 で割当 |
| **失敗** | エラーダイアログ禁止。値は自動補正、Status バーに短い注記 |
| **保存** | 手動のみ。Ctrl+S または ☰ メニュー。dirty 状態は Status バーに `Modified` 印 |
| **Undo** | `UndoSystem::Instance().Undo/Redo` を流用、Ctrl+Z/Y のみで操作。専用 UI なし |

---

## 4. Workbench タブ詳細

各タブは**既存 Draw 関数を中身として流用**。新規 UI は作らない。各 Draw 関数の冒頭・末尾にある説明文 / hint / TextDisabled の冗長な行は**削除**してビジュアル要素のみ残す。

| タブラベル | 流用関数 | 削るもの |
|---|---|---|
| `Move##wb`   | `DrawStateMachinePanel` (グラフ込み) | 冗長な TextDisabled 説明 |
| `Attack##wb` | `DrawTimelinePanel` | プレイヘッド説明文等 |
| `Hit##wb`    | `DrawPersistentColliderSection` | section 説明行 |
| `Input##wb`  | `DrawInputPanel` | 詳細キー説明 |
| `Bone##wb`   | `DrawSkeletonPanel` (コライダ部分は除く) | 説明系の TextDisabled |

**注**: `Bone` と `Hit` が分離するため、`DrawSkeletonPanel` に v1 で統合した永続コライダセクションを**取り除く**。

---

## 5. プレビュー仕様

- **動きを見るだけ**。ダミー敵 / 戦闘 HUD / HP バー / スタミナ — 全部作らない。
- viewport にはプレイヤーモデル + 薄い床グリッドのみ。
- `Test` モードでは WASD / マウスでプレイヤーを動かして動きを確認できる。攻撃ボタンでアニメ + 当たり判定が再生される（敵がいないので空振り）。
- それだけで十分。

---

## 6. 実装フェーズ

各フェーズ完了時点でビルド・起動・目視確認できる粒度。

### Phase 1: テーマ刷新 + Tool ポップオーバー残骸の除去
- `EditorTheme.h` の `PushPlayerEditorPanelStyle` を §2 の不透明値に書き換え
- v1 で半透明に振った設定を全て不透明に戻す
- `EditorTheme` の `WindowRounding` 等のサイズ系も §2.3 の値に揃える
- **検証**: 起動して panel がはっきり不透明で表示される

### Phase 2: Top Bar を UE 風スタックに刷新
- `DrawToolbarStackButton(icon, label, active, onClick)` ヘルパー新設 (64×46px、アイコン上 + ラベル下)
- 5 グループ (Menu / File / Mode / Mode切替 / 補助) に再編、グループ間に 1px 縦線
- `Setup Full Enemy/NPC` / `Repair Runtime` / `Reset` / `Tool:` ラベルを削除
- ラベルは英語 1〜2 単語 + `##tb` suffix
- `Save` の dirty 印 (右上 3px 橙ドット) を実装

### Phase 3: Quick Tools レール (左 56px)
- DockSpace の外側に固定 child window（左端から 56px）
- アイコン 5 + 折畳ボタン
- クリックで Workbench タブ切替・開閉

### Phase 4: Workbench を単一 window 化
- 既存 5 個の DrawXxxPanel を 1 つの window 内 TabBar で呼ぶ
- `DrawSkeletonPanel` から v1 で統合した永続コライダ部分を取り除く
- 元の 7 panel 構成は廃止、Properties は浮遊 Inspector へ

### Phase 5: 浮遊 Inspector
- SelectionContext が None なら描画スキップ
- `SetNextWindowPos` で右端固定
- 中身は既存 `DrawPropertiesPanel` の switch をそのまま使用

### Phase 6: Ctrl+Z / Ctrl+S 配線
- `ImGui::IsKeyChordPressed(Ctrl+Z)` で `UndoSystem::Instance().Undo(*m_registry)` を呼ぶ
- `Ctrl+Y` / `Ctrl+Shift+Z` で Redo
- `Ctrl+S` で `SavePrefabDocument(false)` を呼ぶ
- Status バーに dirty 印 (`Modified`) を表示

**最小完成 = Phase 1〜5**。Phase 6 は次セッションで対応可。

---

## 7. 受け入れ基準

- [ ] パネルは不透明、3 段の輝度差で階層が視認できる
- [ ] Top Bar は UE 風スタックボタン (アイコン上 + ラベル下) で 5 グループに区切られている
- [ ] グループ間に 1px の縦区切り線がある
- [ ] `Save` ボタンは dirty 時に右上に橙ドット印が出る
- [ ] Quick Tools レール = 左 56px 幅、アイコン 5 個 + 折畳
- [ ] Workbench は単一 window で 5 タブ
- [ ] 選択なしのとき Inspector は描画されない
- [ ] ラベルは英語 1〜2 単語、`##suffix` 付与済み
- [ ] パネル内の説明文 / hint / 冗長な TextDisabled が削除されている
- [ ] ツールチップは出ない
- [ ] エラーダイアログを 1 つも出さない
- [ ] `Full Player` 1 クリックで動くプレイヤーが完成（既存挙動を維持）
- [ ] Ctrl+Z でレベルエディタと共通の Undo 履歴を辿れる

---

## 8. 廃止 / 不採用

仕様から**意図的に外したもの**（今後も足さない）:
- 半透明 / ガラスモーフィズム
- オートセーブ
- チュートリアルオーバーレイ
- 複数テンプレカード
- Undo 履歴サムネビュー
- ダミー敵スポナー
- 戦闘 HUD (HP / スタミナ / コンボ / フレームデータ)
- ヘルプボタン
- 設定パネル
- ツールチップ
