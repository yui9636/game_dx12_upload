# Effect System Enhancement Specification v1.1

## 1. 現状のシステム概要

### 1.1 描画モード
| モード | 頂点コスト | 実用上限 | 状態 |
|--------|-----------|----------|------|
| Billboard | 4頂点/particle | ~419万 | 安定 |
| Mesh | モデル依存 | ~数万 | 安定 |
| Ribbon | 28頂点/particle | ~1万 | 安定 |

### 1.2 実装済み機能
- SoA GPU駆動シミュレーション (Hot/Warm/Cold/Header)
- 6種スポーンシェイプ (Point/Sphere/Box/Cone/Circle/Line)
- ページベースアリーナアロケータ (8192粒子/ページ, 最大2048ページ)
- PrefixSum + ScatterAlive による GPU AliveList 構築
- ExecuteIndirect 描画
- Curl Noise / Vortex / Drag / Gravity
- Sub-UV アニメーション
- ソフトパーティクル (深度フェード)
- カラーグラデーション (Start→End lerp)
- 速度ストレッチ (Billboard)
- ノードベースエフェクトグラフエディタ
- パラメータバインディング (Exposed Parameters)

### 1.3 現在の制限事項
- ブレンドモード固定 (Billboard=Premultiplied Alpha, Mesh=Additive)
- テクスチャ1枚/ドローコール
- HDRカラー非対応 (RGBA8パッキング)
- ライティングなし (Billboard/Ribbon)
- 法線マップ/スペキュラなし (Mesh)
- GPU Particle Collision なし
- Attractor/Repeller なし
- ゲームレンダー上での見た目がエディタプレビューと一致しない場合がある

---

## 2. 強化ロードマップ

### Phase 1A: 即効性の高い表現力強化 (優先度: 最高)

#### 2.1 ブレンドモード切り替え
**目的**: エフェクトごとに最適なブレンドを選択可能にする

| ブレンドモード | SrcBlend | DestBlend | 用途 |
|---------------|----------|-----------|------|
| Additive | ONE | ONE | 火花, 光, マジック |
| PremultipliedAlpha | ONE | INV_SRC_ALPHA | 煙, ダスト |
| AlphaBlend | SRC_ALPHA | INV_SRC_ALPHA | 汎用 |
| Multiply | DEST_COLOR | ZERO | 影, ダーク系 |
| SoftAdditive | ONE | INV_SRC_COLOR | 柔らかい発光 |

**実装方針**:
- `EffectParticleSimulationLayout` に `blendMode` フィールド追加
- SpriteRenderer ノードの UI にドロップダウン追加
- PSO を blendMode ごとにキャッシュ (最大5種)
- Pixel Shader は共通、PSO の BlendDesc だけ差し替え

**変更ファイル**: EffectGraphAsset.h, EffectCompiler.cpp, EffectEditorPanel.cpp, EffectParticlePass.cpp

---

#### 2.2 Per-Particle ランダム
**目的**: 均一すぎるパーティクル群に個体差を与える

**仕様**:
- Emit 時に per-particle ランダム係数を Cold に書き込み
  - `randomSeed` (既存 `emitterSeed` を活用)
  - Speed ±30%, Size ±40%, Lifetime ±25% の範囲をエディタで設定
- Update でランダム係数を参照してパラメータに乗算
- Cold の既存 `sizeFadeBias` フィールド (4B) の未使用ビット `fadeBias` にランダム係数をパック

**変更ファイル**: EffectParticleEmit_cs.hlsl, EffectParticleUpdate_cs.hlsl, EffectGraphAsset.h, EffectEditorPanel.cpp

---

#### 2.3 Wind (グローバル風力)
**目的**: 全パーティクルに一律の風を適用

**仕様**:
- `windDirection` (float3) + `windStrength` (float) を CBV に追加
- `windTurbulence` (float): Curl Noise と組み合わせて風の揺らぎを表現
- Update: `velocity += wind * dt`

**変更ファイル**: EffectParticleUpdate_cs.hlsl, EffectParticleRuntimeCommon.hlsli, EffectEditorPanel.cpp

---

### Phase 1B: エディタ体験向上 (優先度: 高)

#### 2.4 リアルタイムパラメータスライダー
**目的**: プレビュー中にパラメータを変更し即座に反映

**仕様**:
- プレビュー中に Exposed Parameter の値を変更可能
- EffectParameterOverrideComponent 経由で即時反映
- パラメータ名をリスト表示、スライダーで調整
- 「触って楽しい」を最優先

**変更ファイル**: EffectEditorPanel.cpp

---

#### 2.5 プリセットライブラリ
**目的**: 再利用可能なエフェクトを保存/ロード

**仕様**:
- JSON シリアライズ/デシリアライズ (既存の EffectGraphAsset をそのまま保存)
- `Data/Effect/Presets/` ディレクトリに保存
- エディタ UI にプリセットブラウザ追加
- サムネイルプレビュー (オフスクリーンレンダリング)
- スポーンシェイプの Gizmo 表示 (Box/Sphere/Cone の輪郭をワイヤフレームで描画)

**変更ファイル**: EffectEditorPanel.cpp, EffectGraphAsset.h (シリアライズ)

---

### Phase 1C: カーブ制御 (優先度: 中高)

#### 2.6 サイズカーブ (マルチキー)
**目的**: サイズの時間変化をカーブで制御

**仕様**:
- 最大4キーの `{ size, normalizedTime }` テーブル
- **per-emitter shared** (CBV 経由)。粒子ごと個別カーブはやらない
- 粒子ごとの差は Emit 時の seed/bias で乗算 (Phase 1A 2.2 のランダムと連携)
- Update シェーダで線形補間

**設計判断**: サイズカーブは全粒子で共通形状。粒子ごとのバリエーションは `randomSizeScale` 乗算で実現。完全な粒子ごと個別カーブはコスト対効果が見合わないためスコープ外とする。

**変更ファイル**: EffectParticleRuntimeCommon.hlsli, EffectParticleUpdate_cs.hlsl, EffectEditorPanel.cpp

---

#### 2.7 カラーグラデーションカーブ (マルチキー)
**目的**: Start→End の2色補間では不十分な表現を可能にする

**仕様**:
- 最大4キーのカラーグラデーション
- 各キー: `{ RGBA8 color, f16 normalizedTime }` = 6バイト/キー

**データレイアウト** (Cold 32B → 48B):
```
// 追加 16B (4キー分を完全格納)
uint   gradientKey01;     // 4B  key0=RGBA8 (time=0.0 固定、始点)
uint   gradientKey23;     // 4B  key3=RGBA8 (time=1.0 固定、終点)
uint   gradientMid0;      // 4B  key1=RGBA8
uint   gradientMid1Times; // 4B  key2=RGBA8_LO16 | time1(f16_HI8) | time2(f16_LO8)
```

**格納ルール**:
- key0 (time=0.0) と key3 (time=1.0) は固定端点
- key1, key2 は中間キー (0 < time1 < time2 < 1)
- 2キーモード (Start/End のみ) の場合: key1=key0, key2=key3, time1=0, time2=1 で既存挙動と同等
- Update シェーダ: normalizedAge で区間を判定し lerp

**エディタUI**: ImGUI カラーグラデーションバー (ドラッグでキー追加/移動)

**変更ファイル**: EffectParticleSoA.hlsli, EffectParticleEmit_cs.hlsl, EffectParticleUpdate_cs.hlsl, EffectEditorPanel.cpp, EffectGraphAsset.h

---

### Phase 2: 物理インタラクション (優先度: 中)

#### 2.8 Attractor / Repeller
**目的**: パーティクルを特定の点に引き寄せる/弾く

**仕様**:
- 最大4個のアトラクタ/リペラー
- 各: `{ position, strength, radius, falloff }` (16バイト)
- CBV 経由で Update シェーダに渡す
- `strength > 0` で引力、`< 0` で斥力
- `falloff`: 0=一定, 1=線形, 2=二乗減衰

**変更ファイル**: EffectParticleUpdate_cs.hlsl, EffectParticlePass.cpp, EffectGraphAsset.h, EffectEditorPanel.cpp

---

#### 2.9 GPU Collision (平面 + 球)
**目的**: パーティクルが地面や障害物に反応

**仕様**:
- コリジョンプリミティブ: 無限平面 (Y=N) + 球 (最大4個)
- CBV でプリミティブ情報を渡す (plane: float4, sphere: float4×4)
- Update シェーダで衝突判定 → 速度反射 + 減衰
- `collisionRestitution` (反発係数), `collisionFriction` (摩擦)
- **early-out**: `collisionEnabled` フラグで判定自体をスキップ可能

**データフロー**:
```
CPU → CBV: { planeNormal, planeD, spheres[4], restitution, friction }
GPU Update: if (dot(pos, plane.xyz) + plane.w < 0) reflect velocity
```

**変更ファイル**: EffectParticleUpdate_cs.hlsl, EffectParticleRuntimeCommon.hlsli, EffectParticlePass.cpp, EffectGraphAsset.h, EffectEditorPanel.cpp

---

#### 2.10 タイムラインビュー
**目的**: エフェクトの時間軸を可視化

**仕様**:
- ImGui カスタムウィジェット: 水平タイムライン
- エミッタのスポーンレート/バーストを時間軸上に表示
- スクラブバーでプレビュー時間を操作
- ループ区間のハイライト

**変更ファイル**: EffectEditorPanel.cpp (新規ウィジェット)

---

### Phase 3: レンダリング品質 (優先度: 低〜中)

#### 2.11 Distortion (屈折/歪み) パーティクル
**目的**: 熱波、衝撃波、水面の揺らぎ

**仕様**:
- 新しい DrawMode: `Distortion`
- 専用ピクセルシェーダ: シーンカラーバッファをサンプルし、法線マップ or パーティクル速度でUVオフセット
- 別パスで描画 (シーンカラーをSRVバインド)
- distortionStrength パラメータ

**工数**: 大。専用パス追加 + シーンカラー SRV バインド + 描画順序制御。

**変更ファイル**: 新規シェーダ, EffectParticlePass.cpp (パス追加), EffectGraphAsset.h

---

#### 2.12 Normal-Mapped Mesh Particles
**目的**: Mesh パーティクルの見た目向上

**仕様**:
- MeshPS に法線マップサンプリング追加
- ルートシグネチャにテクスチャスロット追加 (albedo + normal)
- TBN 計算は VS で行い GS/PS に渡す

**変更ファイル**: EffectParticleMeshVS.hlsl, EffectParticleMeshPS.hlsl, EffectParticlePass.cpp

---

#### 2.13 Light Emitting Particles (発光パーティクル)
**目的**: パーティクル自体が周囲を照らす

**仕様**:
- `emissiveIntensity` パラメータ追加
- Update で alive パーティクルの上位N個 (最大64) をポイントライトとしてバッファに書き出す
- ライティングパスでポイントライトとして参照
- 性能制約: 64ライト上限、影なし

**工数**: 大。CS (LightExtract) + ライティングパス連携 + 上位N抽出ロジック。

**変更ファイル**: 新規 CS (LightExtract), EffectParticlePass.cpp, ライティングパス連携

---

## 3. 別仕様書に分離するエフェクトタイプ

以下は本仕様のスコープ外。個別の設計文書で扱う。

| タイプ | 理由 |
|--------|------|
| **Beam (ビーム/ライトニング)** | ほぼ専用レンダラ。パーティクルSoAとは独立した制御点列ベースの設計が必要 |
| **Decal Particles** | 既存Decalパスとの統合が主題。パーティクルシステムよりDecalシステム側の拡張 |
| **GPU Event (Sub-Emitter)** | パイプライン構造の変更 (EventBuffer, 2段Emit) が必要。影響範囲が広い |

---

## 4. 実装優先順序

| 順序 | 機能 | Phase | 工数 | インパクト |
|------|------|-------|------|-----------|
| 1 | 2.1 ブレンドモード切り替え | 1A | 小 | 大 |
| 2 | 2.2 Per-Particle ランダム | 1A | 小 | 大 |
| 3 | 2.3 Wind | 1A | 小 | 中 |
| 4 | 2.4 リアルタイムパラメータスライダー | 1B | 小 | 大 |
| 5 | 2.5 プリセットライブラリ | 1B | 中 | 大 |
| 6 | 2.6 サイズカーブ | 1C | 小 | 中 |
| 7 | 2.7 カラーグラデーションカーブ | 1C | 中〜大 | 大 |
| 8 | 2.8 Attractor/Repeller | 2 | 小 | 中 |
| 9 | 2.9 GPU Collision | 2 | 中 | 大 |
| 10 | 2.10 タイムラインビュー | 2 | 中 | 中 |
| 11 | 2.11 Distortion | 3 | 大 | 大 |
| 12 | 2.12 Normal-Mapped Mesh | 3 | 中 | 小 |
| 13 | 2.13 Light Emitting | 3 | 大 | 中 |

---

## 5. 機能別性能コスト見積もり

Billboard ベースライン: 419万パーティクル, RTX 3060 想定

| 機能 | Update追加コスト/粒子 | 推定上限 (16ms枠内) | 備考 |
|------|---------------------|---------------------|------|
| **ベースライン (現行)** | — | 419万 | Init/Update/Emit 単一 Dispatch |
| + ブレンド切替 | 0 | 419万 | PSO 切替のみ、シェーダ変更なし |
| + Per-Particle ランダム | +2 ALU | ~400万 | Cold 読み + 乗算 |
| + Wind | +3 ALU | ~400万 | float3 加算 |
| + サイズカーブ (4key) | +8 ALU | ~380万 | CBV 読み + 区間判定 + lerp |
| + カラーカーブ (4key) | +12 ALU | ~350万 | Cold 読み + 区間判定 + RGBA lerp |
| + Attractor x4 | +40 ALU | ~280万 | 4回の距離計算 + 正規化 + 加算 |
| + Collision (plane+sphere4) | +50 ALU | ~230万 | 5回の衝突判定 + 反射ベクトル |
| **全部載せ** | +115 ALU | ~180万 | 全機能有効時の下限目安 |

### 性能予算ルール
- **Phase 1 機能 (1A+1B+1C)**: フレーム追加コスト **+0.1ms 以内** @ 100万パーティクル
- **Phase 2 機能 (Attractor/Collision)**: フレーム追加コスト **+0.3〜1.0ms 許容** @ 100万パーティクル
- **Phase 3 機能 (Distortion/Light)**: 個別プロファイル必須。予算は機能ごとに設定
- Collision/Attractor は `enabled` フラグで分岐し、**無効時はゼロコスト**

### Ribbon 特記
Ribbon は GS maxvertexcount=28 のため、Billboard 比で描画コストが約7倍。
Collision/Attractor 有効時の Ribbon 実用上限は **~5,000** 程度。

---

## 6. SoA データレイアウト拡張計画

### 現行 (v3)
```
Hot:  32B = position(12) + ageLife(4) + velocity(12) + sizeSpin(4)
Warm: 16B = color(4) + endColor(4) + texcoord(4) + flags(4)
Cold: 32B = accel(12) + dragSpin(4) + sizeRange(4) + lifeBias(4) + sizeFadeBias(4) + seed(4)
Header: 8B = slotIndex(4) + packed(4)
Total: 88B/particle
```

### 拡張後 (v4)
```
Hot:  32B (変更なし)
Warm: 16B (変更なし)
Cold: 48B = 既存32B + gradientPacked(16)
Header: 8B (変更なし)
Total: 104B/particle

gradientPacked 内訳 (16B):
  [0] uint gradientKey0     // 4B  RGBA8 (time=0.0 固定、始点)
  [1] uint gradientKey3     // 4B  RGBA8 (time=1.0 固定、終点)
  [2] uint gradientMid0     // 4B  key1 RGBA8
  [3] uint gradientMid1Time // 4B  key2_RG(8) | key2_BA(8) | time1(f16_8bit) | time2(f16_8bit)
```

**格納ルール**:
- key0 (t=0.0) と key3 (t=1.0) は固定端点、CBV ではなく Cold に持つ
- key1, key2 は中間キー。2キーモード時は key1=key0, key2=key3, time1=0, time2=1
- Update シェーダ: normalizedAge で3区間 [0,t1], [t1,t2], [t2,1] を判定して lerp
- **Warm.packedEndColor は廃止せず互換維持**。2キーモードでは従来通り Warm から読む。4キーモードでは Cold の gradientPacked を使用。flags ビットでモード切替。

**メモリ影響**: 419万パーティクルで +67MB (Cold 32B→48B)。許容範囲。

---

## 7. 後方互換性とマイグレーション

### バージョニング
- SoA レイアウトにバージョンフィールドを持たせる
  - `ParticleArenaAllocation` に `uint32_t soaVersion = 3;` 追加
  - v3 = 現行 (Cold 32B), v4 = 拡張 (Cold 48B)

### アセットマイグレーション
- EffectGraphAsset JSON に `"version": 1` フィールド追加
- ロード時に version が古い場合:
  - 不在フィールドはデフォルト値で埋める
  - `gradientKeyCount = 2` (Start/End のみ)
  - `randomSpeedRange = 0`, `randomSizeRange = 0`, `randomLifeRange = 0`
  - `blendMode = PremultipliedAlpha` (現行挙動維持)
  - `windStrength = 0` (風なし)
- バージョン書き込み: 保存時に常に最新 version を書く

### GPU バッファマイグレーション
- Arena 再作成時に soaVersion を判定し、正しい stride でバッファ確保
- 既存セーブデータに GPU バッファは含まれない (毎回 Init で再構築) ため、GPU 側マイグレーションは不要

### デフォルト充填ルール
| フィールド | デフォルト値 | 理由 |
|-----------|-------------|------|
| blendMode | PremultipliedAlpha | 現行の Billboard ブレンド |
| windStrength | 0.0 | 風なし = 現行挙動 |
| randomSpeedRange | 0.0 | ランダムなし = 現行挙動 |
| randomSizeRange | 0.0 | ランダムなし = 現行挙動 |
| randomLifeRange | 0.0 | ランダムなし = 現行挙動 |
| gradientKeyCount | 2 | Start/End のみ = 現行挙動 |
| attractorCount | 0 | アトラクタなし |
| collisionEnabled | false | コリジョン無効 |

---

## 8. テンプレート拡充計画

Phase 1 完了後に追加予定のテンプレート:

| テンプレート名 | モード | スポーンレート | 主要特徴 |
|---------------|--------|-------------|---------|
| Fire Burst | Billboard | 45,000 | Additive + Orange→Red gradient |
| Smoke Ring | Billboard | 15,000 | AlphaBlend + Circle shape + Wind |
| Heal Aura | Billboard | 20,000 | SoftAdditive + Green + Vortex |
| Impact Sparks | Billboard | 80,000 | Additive + Cone + High speed + Short life |
| Snow Fall | Billboard | 5,000 | AlphaBlend + Box shape + Wind + Slow |
| Rain | Ribbon | 2,000 | AlphaBlend + Line + Gravity |

Phase 2 完了後:
| テンプレート名 | モード | スポーンレート | 主要特徴 |
|---------------|--------|-------------|---------|
| Waterfall | Billboard | 30,000 | AlphaBlend + Collision (plane) |
| Gravity Well | Billboard | 50,000 | Additive + Attractor |

---

## 9. 非機能要件

### 性能予算
- Phase 1 機能: +0.1ms 以内 @ 100万パーティクル
- Phase 2 機能: +0.3〜1.0ms 許容 @ 100万パーティクル
- Phase 3 機能: 個別プロファイル必須

### メモリ予算
- GPU メモリ使用量は現行比 +30% 以内
- Cold 拡張 (32B→48B) で +50% 増だが、Hot/Warm/Header は据え置きのため全体では +18%

### エディタ操作性
- 3クリック以内で主要パラメータにアクセス可能
- プレビュー中のパラメータ変更は 1フレーム以内に反映
- スポーンシェイプは Gizmo 表示で直感的に確認可能

### 後方互換
- v3 アセットは無変更でロード可能 (デフォルト充填)
- バイナリ互換を破る場合は version フィールドで判別
- GPU バッファは毎フレーム Init 再構築のため、マイグレーション不要
