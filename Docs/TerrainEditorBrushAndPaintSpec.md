# Terrain Editor Brush And Paint Spec

## 目的

Terrain を「表示できる板」から、レベルエディター上で直感的に形状と見た目を作れる地形編集機能へ拡張する。

今回の主眼は、Scene View 上でブラシを使って地形を編集できる状態にすること。緑一色の仮表示は、最低限の高さ・傾斜ベース表示を経由し、後続の Paint / Layer 編集へ自然に繋がる構造にする。

## 現状の問題

- Terrain Editor の Sculpt / Paint UI は存在するが、Scene View のマウス操作に接続されていない。
- `ApplyBrush` はあるが、クリック位置の Terrain ヒット判定、ドラッグ適用、ブラシプレビューがない。
- 表示は procedural な緑系に寄っていて、地形としての読みやすさが低い。
- Terrain の編集結果は `heightData` に乗るが、操作体験として「どこに、どの範囲で、どれだけ効くか」が見えない。
- 編集後の物理・描画再生成の流れはあるが、ブラシ操作単位での扱いがまだ粗い。

## 完成条件

Phase 1 完了時点で以下を満たす。

- Terrain Editor の Sculpt タブを開いた状態で、Scene View 上の Terrain を左ドラッグして編集できる。
- ブラシ位置と半径が Scene View に円で表示される。
- Raise / Lower / Smooth / Flatten が実際に heightData に反映される。
- ブラシ適用後に terrain mesh と terrain physics が再生成される。
- Terrain 外をクリックしても誤編集されない。
- Gizmo 操作や通常選択とブラシ操作が衝突しない。
- `Debug x64` でビルドが通る。

Phase 2 完了時点で以下を満たす。

- 高さと傾斜で色が変わり、緑一色より地形の起伏が読みやすくなる。
- Paint タブで Layer 0-3 の重みを塗れる土台ができる。
- Layer 未設定でも破綻せず、既定色で表示される。

Phase 3 完了時点で以下を満たす。

- Terrain の Layer 設定と splatData が scene / prefab 保存対象として扱える。
- ブラシ編集を Undo / Redo できる最小単位が定義される。
- 保存後に開き直しても地形の形状と塗りが復元される。

## 操作仕様

### 基本操作

- Terrain エンティティを選択すると Terrain Editor を開ける。
- Sculpt タブが有効な間だけ Scene View のブラシ編集を有効にする。
- 左ドラッグでブラシを連続適用する。
- 左クリック単発でも 1 回適用する。
- Alt / 右クリック / 中クリック中はカメラ操作を優先し、ブラシは動かさない。
- ImGuizmo が hover / using 状態のときは Gizmo を優先し、ブラシは動かさない。

### ブラシ表示

- マウスが Terrain 上にあるとき、Scene View 上に円形プレビューを表示する。
- 円は Terrain 表面に沿う厳密な 3D 円ではなく、まずはワールド XZ 平面上の円を Scene View に投影する。
- 円色は Sculpt では水色、Paint では黄色系にする。
- Terrain 外ではブラシ円を出さない。

### Sculpt モード

- Raise: heightData を上げる。
- Lower: heightData を下げる。
- Smooth: 周辺平均との差へ寄せる。
- Flatten: `targetHeight` へ寄せる。

各モードは以下の共通パラメータを使う。

- Radius: ワールド単位の半径。
- Strength: 1 ストロークあたりの効果量。
- Falloff: 中心から外側へ弱くなるカーブ。

### Paint モード

- Paint タブでは `layerIndex` を選び、splatData の対象チャンネルへ重みを塗る。
- 他レイヤーは正規化し、合計が 255 相当になるようにする。
- レイヤー未設定でも Layer 0 は既定色として扱う。

## データ仕様

### TerrainAsset

- `heightData`: 既存の 0-1 height map。
- `splatData`: resolution * resolution * 4 の RGBA layer weight。
- `layers`: 最大 4 レイヤーの見た目設定。

今後追加候補:

- `brushStampVersion`: Undo/Redo や dirty 判定用。
- `materialMode`: Procedural / LayerPaint の切り替え。

### TerrainComponent

- `needsRebuild`: heightData / splatData 変更時に true。
- `showInEditor`: 既存通り。

追加候補:

- `editable`: Scene View ブラシ対象にするか。
- `collisionEnabled`: terrain physics 登録対象にするか。

## Scene View 連携

### ヒット判定

1. Scene View の mouse position から world ray を作る。
2. 選択中 Terrain の XZ 範囲と ray を交差させる。
3. 交差点の XZ から heightData をサンプルし、Terrain 表面上の hit point を得る。
4. hit point が範囲内ならブラシ対象にする。

Jolt raycast だけに依存しない。Terrain の編集対象判定は TerrainAsset 自身から計算する。これにより、物理登録の有無や更新遅延に影響されず編集できる。

### 通常選択との優先順位

優先順位は以下。

1. Scene View toolbar
2. ImGuizmo
3. Terrain brush
4. UI / mesh picking

Terrain brush が有効なドラッグ中は `HandleScenePicking` を走らせない。

## 描画仕様

### Phase 1

- 既存 shader の procedural color を維持する。
- ブラシ円は ImGui foreground draw list で描く。

### Phase 2

- 高さに応じて低地・中腹・高地の色を分ける。
- 法線の傾きで岩場寄りの色を混ぜる。
- splatData が有効なら layer weight を使う。

### Phase 3

- Albedo texture の layer blending を有効にする。
- Normal / roughness は後続対応でよい。

## Undo / Redo

Phase 1 では Undo は必須にしない。  
Phase 3 で以下を実装する。

- 1 ドラッグ開始時に変更前 heightData / splatData の範囲を保存。
- ドラッグ終了時に変更後との差分を UndoSystem へ積む。
- 差分範囲はブラシ半径の影響範囲だけに限定する。

## 実装フェーズ

### Phase 1: Sculpt Brush

- TerrainEditorPanel に現在の tool 状態を公開する。
- EditorLayer Scene View から Terrain brush hit を計算する。
- 左クリック / 左ドラッグで `ApplyBrush` を呼ぶ。
- Smooth を実装する。
- ブラシ円プレビューを描く。
- 適用後に `needsRebuild = true`。

### Phase 2: Terrain Visual Readability

- Terrain shader の高さ・傾斜色を調整する。
- Editor 上で Layer なしでも地形形状が読みやすい色にする。
- Paint タブの layerIndex と splatData 更新を実装する。

### Phase 3: Layer Paint And Save

- splatData / layers の保存と読み込みを追加する。
- Layer 0-3 の texture path を保存する。
- Undo / Redo の最小対応を追加する。

## 非対応範囲

今回すぐには以下をやらない。

- 自動生成地形の高度な erosion。
- ランタイム中の terrain 編集。
- Terrain chunk の部分更新最適化。
- 高度な texture streaming。
- NavMesh 生成。
- Jolt character controller への全面移行。

## 注意点

- Terrain 編集は Level Editor の Scene View 操作として扱う。Terrain Editor パネル内だけで完結させない。
- UI は細かい数値入力だけに寄せず、Scene View 上の直接操作を主役にする。
- 物理登録は編集後に遅れて更新されてもよいが、最終的に player が編集後地形に乗ることを保証する。
- 既存の mesh picking / UI placement / gizmo 操作を壊さない。
