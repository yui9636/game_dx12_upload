# MeshEffect Variant System 仕様書
**作成日**: 2026-04-14  
**ステータス**: 計画中  
**目的**: メッシュを起点としたリッチエフェクト（剣戟・魔法・環境）を実現するシェーダーバリアントシステムの設計・実装計画

---

## 1. 背景と課題

### 1.1 旧システム（DX11）の強み
`ShaderCompiler::CompilePixelShader()` により、17種のフラグ（Dissolve / FlowMap / Fresnel / MatCap 等）を**ランタイム組み合わせ**でシェーダーバリアント生成。  
これにより剣戟メッシュのフェードアウト・テクスチャ流れ・エッジ発光などを自在に組み合わせて表現できた。

### 1.2 現DX12システムの問題
- `EffectParticleMeshPS` は単一固定 PSO のみ
- `shaderVariantKey` / `shaderId` フィールドは存在するが未接続
- Dissolve・FlowMap・Fresnel 等が**一切使えない**
- メッシュとパーティクルの**連動機能がない**

### 1.3 目標
DX12 環境でビルド時プリコンパイル方式により旧システム同等以上のバリアントシステムを再構築し、  
**メッシュを起点としたあらゆるエフェクト表現**を可能にする。

---

## 2. 対象エフェクトカタログ

### 2.1 剣戟系
| エフェクト | 必要機能 |
|---|---|
| 斬撃メッシュ（弧状フェード） | Dissolve + DissolveGlow + AlphaFade |
| 刃のテクスチャ流れ | FlowMap |
| 刃エッジ発光 | Fresnel |
| 衝撃波（歪み） | Distort |
| 斬撃の余韻（サイドフェード） | SideFade |
| 火花パーティクル連動 | MeshToParticle Spawn |

### 2.2 魔法系
| エフェクト | 必要機能 |
|---|---|
| 魔法陣（回転UV） | FlowMap + Toon |
| 召喚メッシュ出現 | Dissolve + NormalMap |
| オーラ（脈動フレネル） | Fresnel + AlphaFade |
| 爆発（クロマティック） | ChromaticAberration + Distort |
| グラデーションマップ炎 | GradientMap + FlowMap |

### 2.3 環境・ギミック系
| エフェクト | 必要機能 |
|---|---|
| 地面亀裂（マスク＋発光） | Mask + DissolveGlow |
| 氷結（MatCap+フレネル） | MatCap + Fresnel + NormalMap |
| 毒沼（フローUV） | FlowMap + SubTexture |
| トゥーンアウトライン発光 | Toon + Fresnel |

---

## 3. シェーダーバリアントシステム設計

### 3.1 フラグ定義（旧システム互換＋拡張）

```cpp
enum EffectMeshShaderFlags : uint32_t
{
    MeshFlag_None               = 0,
    MeshFlag_Texture            = 1 << 0,   // ベーステクスチャ
    MeshFlag_Dissolve           = 1 << 1,   // ディゾルブ
    MeshFlag_Distort            = 1 << 2,   // UV歪み
    MeshFlag_Lighting           = 1 << 3,   // ライティング
    MeshFlag_Mask               = 1 << 4,   // マスクテクスチャ
    MeshFlag_Fresnel            = 1 << 5,   // フレネル発光
    MeshFlag_Flipbook           = 1 << 6,   // フリップブックアニメ
    MeshFlag_GradientMap        = 1 << 7,   // グラデーションマップ
    MeshFlag_ChromaticAberration = 1 << 8,  // 色収差
    MeshFlag_DissolveGlow       = 1 << 9,   // ディゾルブ縁発光
    MeshFlag_MatCap             = 1 << 10,  // MatCapリフレクション
    MeshFlag_NormalMap          = 1 << 11,  // 法線マップ
    MeshFlag_FlowMap            = 1 << 12,  // フローマップUVアニメ
    MeshFlag_SideFade           = 1 << 13,  // サイドフェード（辺縁α減衰）
    MeshFlag_AlphaFade          = 1 << 14,  // ライフタイムαフェード
    MeshFlag_SubTexture         = 1 << 15,  // サブテクスチャ合成
    MeshFlag_Toon               = 1 << 16,  // トゥーンシェーディング
    // 新規追加
    MeshFlag_RimLight           = 1 << 17,  // リムライト
    MeshFlag_VertexColorBlend   = 1 << 18,  // 頂点カラー乗算
    MeshFlag_Emission           = 1 << 19,  // エミッションテクスチャ
    MeshFlag_Scroll             = 1 << 20,  // UVスクロール
};
```

### 3.2 プリコンパイル戦略
DX12 はランタイムコンパイル非推奨のため、**ビルド時にdxcで頻出組み合わせをCSO生成**。

**プリコンパイル済み組み合わせ（剣戟・魔法に特化した頻出セット）:**

```
# 剣戟セット
EffectMeshPS_Slash_Basic      = Texture + Dissolve + AlphaFade
EffectMeshPS_Slash_Glow       = Texture + Dissolve + DissolveGlow + AlphaFade
EffectMeshPS_Slash_Flow       = Texture + FlowMap + Fresnel + AlphaFade
EffectMeshPS_Slash_Full       = Texture + Dissolve + DissolveGlow + FlowMap + Fresnel + SideFade + AlphaFade

# 魔法セット
EffectMeshPS_Magic_Circle     = Texture + FlowMap + Mask + AlphaFade
EffectMeshPS_Magic_Summon     = Texture + Dissolve + NormalMap + Lighting
EffectMeshPS_Magic_Aura       = Texture + Fresnel + AlphaFade + RimLight
EffectMeshPS_Magic_Explosion  = Texture + ChromaticAberration + Distort + AlphaFade

# 汎用
EffectMeshPS_Universal_Glow   = Texture + Fresnel + Emission + AlphaFade
EffectMeshPS_Universal_Flow   = Texture + FlowMap + SubTexture + AlphaFade
```

**命名規則:** `EffectMeshPS_{variantKey:08x}.cso`（フラグを16進数）  
→ 任意フラグ組み合わせも対応可能

### 3.3 PSO キャッシュシステム

```cpp
// EffectParticlePass.cpp 内
struct MeshVariantPSO
{
    uint32_t variantKey;
    ComPtr<ID3D12PipelineState> pso;
};

std::vector<MeshVariantPSO> m_meshVariantPSOs;

ID3D12PipelineState* GetOrCreateMeshVariantPSO(uint32_t variantKey);
```

初回使用時に `.cso` をロードして PSO 生成、以降キャッシュから取得。

---

## 4. HLSLシェーダー設計

### 4.1 EffectMeshVariantPS.hlsl（マスターシェーダー）

```hlsl
// --- テクスチャ宣言 ---
Texture2D gBaseTexture   : register(t0);  // ベース
Texture2D gMaskTexture   : register(t1);  // マスク / ディゾルブノイズ
Texture2D gNormalMap     : register(t2);  // 法線マップ
Texture2D gFlowMap       : register(t3);  // フローマップ
Texture2D gGradientMap   : register(t4);  // グラデーションマップ
Texture2D gSubTexture    : register(t5);  // サブテクスチャ
Texture2D gEmissionTex   : register(t6);  // エミッションマップ
TextureCube gMatCap      : register(t7);  // MatCap

// --- cbuffer ---
cbuffer CbMeshEffect : register(b2)
{
    float  gDissolveAmount;    // 0..1
    float  gDissolveEdge;      // 発光幅
    float4 gDissolveGlowColor; // 発光色
    float  gFresnelPower;
    float4 gFresnelColor;
    float2 gFlowSpeed;         // UV流れ速度
    float  gFlowStrength;
    float  gAlphaFade;         // ライフタイムα
    float2 gScrollSpeed;       // UVスクロール
    float  gDistortStrength;
    float4 gRimColor;
    float  gRimPower;
    float4 gEmissionColor;
    float  gEmissionIntensity;
    // ... padding
};

float4 main(PS_IN pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

#ifdef USE_SCROLL
    uv += gScrollSpeed * gTime;
#endif

#ifdef USE_FLOW_MAP
    float2 flow = gFlowMap.Sample(gSampler, uv).rg * 2.0 - 1.0;
    uv += flow * gFlowStrength * gTime;
#endif

    float4 color = float4(1,1,1,1);
#ifdef USE_TEXTURE
    color = gBaseTexture.Sample(gSampler, uv);
#endif

#ifdef USE_NORMAL_MAP
    // 法線変換 → ライティング計算
#endif

#ifdef USE_DISSOLVE
    float noise = gMaskTexture.Sample(gSampler, uv).r;
    float edge  = smoothstep(gDissolveAmount - gDissolveEdge, gDissolveAmount, noise);
    #ifdef USE_DISSOLVE_GLOW
    color.rgb = lerp(color.rgb, gDissolveGlowColor.rgb, edge * (1.0 - smoothstep(gDissolveAmount, gDissolveAmount + gDissolveEdge, noise)));
    #endif
    color.a  *= step(gDissolveAmount, noise);
#endif

#ifdef USE_FRESNEL
    float fresnel = pow(1.0 - saturate(dot(pin.normal, pin.viewDir)), gFresnelPower);
    color.rgb += gFresnelColor.rgb * fresnel;
    color.a   += gFresnelColor.a  * fresnel;
#endif

#ifdef USE_SIDE_FADE
    float sideFade = 1.0 - abs(pin.texcoord.x * 2.0 - 1.0);
    color.a *= sideFade * sideFade;
#endif

#ifdef USE_ALPHA_FADE
    color.a *= gAlphaFade;
#endif

    return color;
}
```

### 4.2 EffectMeshVariantVS.hlsl

既存 `EffectParticleMeshVS.hlsl` ベースに以下を追加:
- ワールド法線・接線の出力（NormalMap / Fresnel 用）
- 頂点カラー出力（VertexColorBlend 用）

---

## 5. cbuffer 拡張設計

### 5.1 EffectMeshEffectConstants（新規）

```cpp
struct EffectMeshEffectConstants
{
    // Dissolve
    float dissolveAmount    = 0.0f;
    float dissolveEdge      = 0.05f;
    DirectX::XMFLOAT2 _pad0;
    DirectX::XMFLOAT4 dissolveGlowColor = { 1.0f, 0.6f, 0.1f, 1.0f };

    // Fresnel
    float fresnelPower      = 3.0f;
    DirectX::XMFLOAT3 _pad1;
    DirectX::XMFLOAT4 fresnelColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // FlowMap
    DirectX::XMFLOAT2 flowSpeed     = { 0.1f, 0.0f };
    float flowStrength  = 0.3f;
    float _pad2;

    // UV Scroll
    DirectX::XMFLOAT2 scrollSpeed   = { 0.0f, 0.0f };
    float alphaFade     = 1.0f;    // ライフタイム連動 0..1
    float distortStrength = 0.0f;

    // Rim
    DirectX::XMFLOAT4 rimColor      = { 1.0f, 1.0f, 1.0f, 1.0f };
    float rimPower      = 2.0f;
    DirectX::XMFLOAT3 _pad3;

    // Emission
    DirectX::XMFLOAT4 emissionColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float emissionIntensity = 0.0f;
    DirectX::XMFLOAT3 _pad4;

    float time          = 0.0f;    // gTime (EffectSystems から注入)
    DirectX::XMFLOAT3 _pad5;
};
static_assert(sizeof(EffectMeshEffectConstants) % 16 == 0, "16byte align required");
```

---

## 6. メッシュ↔パーティクル連動設計

### 6.1 概念：MeshSpawnSurface

メッシュの**サーフェス上にパーティクルをスポーン**するEmitter Shapeの追加。

```
EffectSpawnShapeType::MeshSurface  // メッシュ面上ランダム分布
EffectSpawnShapeType::MeshEdge     // メッシュ辺上（剣の軌跡に沿う等）
EffectSpawnShapeType::MeshVertex   // 頂点上
```

**用途:**
- 斬撃メッシュの面から火花をスポーン
- 魔法陣の辺に沿って粒子を流す
- 氷結メッシュの頂点から破片を飛ばす

### 6.2 概念：MeshToParticleEvent

メッシュエフェクトの**特定タイミング**でパーティクルをバースト生成。

```
DissolveThreshold: dissolveAmount が閾値を超えたとき → パーティクルバースト
LifetimeEnd:       メッシュエフェクト終了時 → パーティクル散布
AlphaThreshold:    α が閾値以下になったとき → 消滅パーティクル
```

---

## 7. エディタ設計

### 7.1 MeshEffectRenderer ノードの拡張

既存の `MeshRenderer` ノードインスペクターに以下タブを追加:

```
[Basic] [Variant Flags] [Dissolve] [FlowMap] [Fresnel] [Emission] [Spawn Link]
```

**Variant Flags タブ:**
- 各フラグのチェックボックス
- ON にすると対応する設定セクションが表示

**Spawn Link タブ:**
- 連動するパーティクルエミッタの選択
- スポーン形状（MeshSurface / MeshEdge / MeshVertex）
- トリガー条件（DissolveThreshold / LifetimeEnd 等）

### 7.2 プリセット剣戟テンプレート

エフェクト新規作成ダイアログに剣戟テンプレートを追加:

| テンプレート名 | 構成 |
|---|---|
| Slash_Basic | 斬撃メッシュ(Dissolve) + 火花Billboard |
| Slash_Glow  | 斬撃メッシュ(DissolveGlow+FlowMap) + 火花 + Trail |
| Magic_Circle | 魔法陣メッシュ(FlowMap+Mask) + 粒子放出 |
| Impact_Burst | 衝撃メッシュ(Distort+ChromaticAberration) + 爆発Billboard |

---

## 8. 実装フェーズ計画

### Phase A: コア（最優先）
| # | タスク | 工数 |
|---|---|---|
| A1 | `EffectMeshVariantPS.hlsl` 作成（全フラグ対応） | 大 |
| A2 | `EffectMeshVariantVS.hlsl` 作成（法線・頂点カラー追加） | 小 |
| A3 | ビルドスクリプト：頻出10セットのCSO生成 | 小 |
| A4 | `EffectMeshEffectConstants` cbuffer 追加 | 小 |
| A5 | `EffectParticlePass.cpp` の PSO キャッシュシステム | 中 |
| A6 | `EffectMeshRendererDescriptor` にフラグ・パラメータ追加 | 小 |
| A7 | `EffectCompiler.cpp` でノードからフラグ/パラメータ抽出 | 中 |

### Phase B: エディタUI
| # | タスク | 工数 |
|---|---|---|
| B1 | MeshRenderer ノードインスペクター拡張（フラグUI） | 中 |
| B2 | Dissolve / FlowMap / Fresnel パラメータスライダー | 小 |
| B3 | 剣戟プリセットテンプレート追加 | 小 |

### Phase C: メッシュ↔パーティクル連動
| # | タスク | 工数 |
|---|---|---|
| C1 | `MeshSurface` スポーン形状の CPU 実装 | 大 |
| C2 | `MeshToParticleEvent` トリガーシステム | 大 |
| C3 | エディタ Spawn Link UI | 中 |

### Phase D: 追加表現
| # | タスク | 工数 |
|---|---|---|
| D1 | Arc スポーン形状（扇形配置） | 中 |
| D2 | Mesh パーティクルの回転制御強化 | 小 |
| D3 | UV スクロールを Ribbon/Billboard にも適用 | 小 |

---

## 9. ファイル変更一覧

| ファイル | 変更内容 |
|---|---|
| `Shader/EffectMeshVariantPS.hlsl` | **新規** マスターシェーダー |
| `Shader/EffectMeshVariantVS.hlsl` | **新規** 対応VSシェーダー |
| `Scripts/CompileMeshVariants.bat` | **新規** バリアント一括コンパイル |
| `Source/EffectRuntime/EffectGraphAsset.h` | MeshRendererDescriptor 拡張 |
| `Source/EffectRuntime/EffectCompiler.cpp` | フラグ抽出追加 |
| `Source/EffectRuntime/EffectSystems.cpp` | time / alphaFade 注入 |
| `Source/RenderContext/RenderQueue.h` | パケットにフラグ/定数追加 |
| `Source/RenderPass/EffectParticlePass.cpp` | PSO キャッシュ + cbuffer 送信 |
| `Source/EffectEditor/EffectEditorPanel.cpp` | フラグUI + プリセット追加 |

---

## 10. 依存関係と注意点

- **DX11 ShaderCompiler.cpp は使用しない** — DX12 では `D3DCompileFromFile` 非推奨
- **バリアント爆発の防止** — 使用フラグ組み合わせは `CompileMeshVariants.bat` で管理、未登録の組み合わせはフォールバック PSO を使用
- **cbuffer 16byte アライメント** — `EffectMeshEffectConstants` は必ず `static_assert` で確認
- **テクスチャスロット衝突** — 既存の `t0`(baseColor), `t1`(curlNoise) と衝突しないようレジスタ割り当て要注意
- **ライフタイムとdissolveの連動** — `alphaFade` は `EffectSystems.cpp` でライフタイム比率から自動計算して注入

---

*本仕様書は実装開始前に変更される可能性があります。Phase A → B → C → D の順で段階的に実装します。*
