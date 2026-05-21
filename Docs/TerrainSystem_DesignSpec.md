# 地形システム発展仕様書

バージョン 1.0 — 2026-05-15  
対象エンジン: MyEngine (DX12)  
担当: 唯飛 中野

---

## 0. 概要

現在の地形システムは「緑一色の地形が描画される」最小実装である。  
本仕様書ではこれを**草・地面・水・山・岩**を自動配置できるモダン地形システムへ発展させるための設計を定義する。

### 現状スナック (Phase 0 — 完了)

| 項目 | 状態 |
|------|------|
| チャンク分割メッシュ生成 | ✅ 完了 (8×8, 512×512) |
| 法線計算 (有限差分) | ✅ 完了 |
| Jolt 物理 HeightField | ✅ 完了 |
| TerrainRenderPass (描画) | ✅ 完了 |
| スプラットマップデータ構造 | ✅ 完了 (uint8 RGBA 4レイヤー) |
| スプラットマップのシェーダー利用 | ❌ 未実装 (PS は固定緑) |
| テクスチャレイヤー (草/岩/雪等) | ❌ 未実装 |
| 水面 | ❌ 未実装 |
| LOD | ❌ スケルトンのみ (常に LOD0) |
| 自動テクスチャ配置 | ❌ 未実装 |

---

## 1. 発展目標

1. **スプラットマップ描画** — 4テクスチャレイヤー（草・地面・岩・雪）をスプラット重みでブレンドしてシェーダーで描画する
2. **自動スプラット生成** — 高度・傾斜から自動的にスプラット重みを計算してエディタ1ボタンで生成できる
3. **水面** — 平面ウォーターパスを追加し、高度しきい値以下を水で覆う
4. **LOD** — 距離に応じてチャンクのメッシュ解像度を段階的に下げる (LOD0〜LOD4)
5. **エディタ強化** — ブラシペイント実装、自動テクスチャボタン、水面高さスライダー

---

## 2. フェーズ計画

| フェーズ | タイトル | 内容 | 工数目安 |
|---------|---------|------|---------|
| **Phase 1** | スプラットシェーダー | TerrainPS.hlsl に4レイヤーブレンド実装 + GPU テクスチャバインド | 1〜2日 |
| **Phase 2** | 自動スプラット生成 | 高度・傾斜ルールベースで splatData を CPU で自動計算 | 0.5日 |
| **Phase 3** | 水面 | WaterRenderPass + 水面パラメータ UI | 1〜2日 |
| **Phase 4** | LOD | 距離別チャンク解像度切り替え + GPU バッファキャッシュ | 2〜3日 |
| **Phase 5** | エディタ強化 | ブラシペイント実装 + ビジュアルフィードバック | 1日 |

---

## 3. Phase 1 — スプラットシェーダー

### 3.1 目的

現在 TerrainPS.hlsl は固定の緑色を出力する（またはテクスチャ未バインドで黒）。  
4枚のアルベド/法線テクスチャをスプラットマップ重みでブレンドして描画する。

### 3.2 スプラットマップ形式

既存の `TerrainAsset::splatData` は `uint8_t RGBA` の平坦配列。  
- R = Layer0 重み (0〜255)  
- G = Layer1 重み  
- B = Layer2 重み  
- A = Layer3 重み  
合計は 255 に正規化されていなくてもよい（シェーダーで合計で割る）。

GPU には `Texture2D<float4>` として 1 枚のスプラットテクスチャを送る。

### 3.3 シェーダー変更

#### TerrainVS.hlsl (変更なし)

現行の入力レイアウト（POSITION / NORMAL / TEXCOORD）で十分。  
UV はチャンク全体で 0〜1 にスケールされているため、世界UV はチャンクオフセットから再計算する。

#### TerrainPS.hlsl — 新規実装

```hlsl
// ---- テクスチャスロット ----
Texture2D    gSplatMap    : register(t0);   // スプラットマップ RGBA
Texture2D    gAlbedo[4]   : register(t1);   // t1, t2, t3, t4 — 各レイヤーアルベド
Texture2D    gNormal[4]   : register(t5);   // t5, t6, t7, t8 — 各レイヤー法線
SamplerState gLinearWrap  : register(s0);
SamplerState gPointClamp  : register(s1);

// ---- 定数バッファ ----
cbuffer TerrainCB : register(b0) {
    float4x4 viewProj;
    float4   chunkOffset;
    float    heightScale;
    float3   _pad;
};

cbuffer TerrainMaterialCB : register(b1) {
    float4 layerTileScale;   // 各レイヤーのタイルスケール
};

// ---- メイン ----
float4 main(PSInput input) : SV_Target
{
    // スプラット重みを取得
    float4 splat = gSplatMap.Sample(gLinearWrap, input.uv);
    float  wSum  = splat.r + splat.g + splat.b + splat.a + 1e-5;
    float4 w     = splat / wSum;   // 正規化

    // 各レイヤーのアルベドをタイルスケール付きでサンプリング
    float3 albedo = float3(0,0,0);
    [unroll] for (int i = 0; i < 4; ++i) {
        float2 tiledUV = input.uv * layerTileScale[i];
        albedo += gAlbedo[i].Sample(gLinearWrap, tiledUV).rgb * w[i];
    }

    // 法線マップブレンド (tangent space)
    float3 normal = float3(0,0,0);
    [unroll] for (int i = 0; i < 4; ++i) {
        float2 tiledUV = input.uv * layerTileScale[i];
        float3 n = gNormal[i].Sample(gLinearWrap, tiledUV).rgb * 2.0 - 1.0;
        normal += n * w[i];
    }
    normal = normalize(normal);
    // world normal: TBN 変換は頂点シェーダーで事前計算するか、次フェーズで追加

    // 簡易ライティング (フォン拡散光)
    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
    float  diff     = saturate(dot(normalize(input.normal), lightDir));
    float3 color    = albedo * (0.2 + 0.8 * diff);

    return float4(color, 1.0);
}
```

### 3.4 C++ 側の変更

#### TerrainRenderPass — テクスチャバインド追加

**TerrainRenderPass.h** に追加するフィールド:

```cpp
// テクスチャ / サンプラー
std::unique_ptr<ITexture>  m_splatTexture;
std::unique_ptr<ITexture>  m_albedoTextures[4];
std::unique_ptr<ITexture>  m_normalTextures[4];
std::unique_ptr<ITexture>  m_defaultWhite;    // バインドなしの代替
std::unique_ptr<IBuffer>   m_cbMaterial;
```

**Execute()** — チャンクごとにテクスチャを更新:

```cpp
// スプラットマップをチャンクの親エンティティごとに更新
cmd->PSSetTexture(0, dc.splatTexture);          // t0
for (int i = 0; i < 4; ++i) {
    cmd->PSSetTexture(1 + i, dc.albedoTextures[i] ?: m_defaultWhite.get());
    cmd->PSSetTexture(5 + i, dc.normalTextures[i] ?: m_defaultWhite.get());
}
cmd->PSSetConstantBuffer(1, m_cbMaterial.get());
```

#### TerrainExtractSystem — draw call にテクスチャポインタを追加

`TerrainChunkDrawCall` 構造体に追加:

```cpp
ITexture*  splatTexture       = nullptr;
ITexture*  albedoTextures[4]  = {};
ITexture*  normalTextures[4]  = {};
float      layerTileScales[4] = {4,4,4,4};
```

#### TerrainAsset — GPU テクスチャキャッシュ追加

`TerrainAsset` に以下を追加:

```cpp
// GPU リソースキャッシュ (Build 時に生成)
std::unique_ptr<ITexture> gpuSplatMap;
std::unique_ptr<ITexture> gpuAlbedo[4];
std::unique_ptr<ITexture> gpuNormal[4];
bool texturesDirty = true;
```

`TerrainBuildSystem::RebuildEntity()` でスプラットマップを GPU テクスチャとしてアップロードする。

### 3.5 デフォルトレイヤー定義

CreateTerrain 時に以下の 3 レイヤーをデフォルト設定する（雪は使用しない）:

| Layer | 名前 | Albedo (PNG) | タイルスケール | 使用条件 |
|-------|------|-------------|--------------|---------|
| 0 | Grass | `Data/Model/terrain/wispy-grass-meadow_albedo.png` | 8.0 | 低高度・低傾斜 |
| 1 | Dirt | `Data/Model/terrain/rocky_dirt1-albedo.png` | 6.0 | 中高度・中傾斜 |
| 2 | Rock | `Data/Model/terrain/layered-rock1-albedo.png` | 4.0 | 高傾斜・高高度 |

**法線マップについて**: Phase 1 ではテクスチャ法線マップを使用せず、  
頂点シェーダーで計算した頂点法線をそのまま使う。  
法線マップは Phase 2 以降で `*_normal.png` を用意して追加する。

テクスチャが存在しない場合は 1×1 白テクスチャをバインドしてクラッシュを防ぐ。

**ロードパス (C++):**

```cpp
// GpuResourceUtils::LoadImageFromFile が PNG/DDS/HDR を自動判別して読み込む
DirectX::ScratchImage image;
DirectX::TexMetadata  meta;
GpuResourceUtils::LoadImageFromFile(layer.albedoPath.c_str(), image, meta);
auto tex = factory->CreateTextureFromMemory(image, meta);
```

---

## 4. Phase 2 — 自動スプラット生成

### 4.1 目的

エディタの「Generate」タブに「**Auto-Paint Splat**」ボタンを追加し、  
高度・傾斜に基づいてスプラットウェイトを自動計算する。

### 4.2 アルゴリズム（3 レイヤー版、雪なし）

```
入力: heightData (0〜1 正規化高度)
      法線から傾斜角 slope を計算

Layer0 (Grass)  重み = (1 - slopeWeight) * (1 - highAltWeight)
Layer1 (Dirt)   重み = slopeWeight * 0.4  (中傾斜で遷移)
Layer2 (Rock)   重み = slopeWeight * 0.6 + highAltWeight
※ 合計 = 1 になるよう正規化

パラメータ (UI から調整可):
  rockAltMin     = 0.6  (岩の開始高度、正規化)
  slopeDeg       = 30.0 (岩/土判定の傾斜角閾値)
```

### 4.3 C++ 実装場所

`TerrainAsset` に以下のメソッドを追加:

```cpp
struct AutoSplatParams {
    float rockAltMin  = 0.6f;   // 正規化高度 (岩の開始)
    float slopeDeg    = 30.0f;  // 岩/土判定の傾斜角閾値 (度)
};

void GenerateAutoSplat(const AutoSplatParams& params);
```

`GenerateAutoSplat()` は全ピクセルを走査して `splatData` を上書きし、  
`TerrainComponent::needsRebuild = true` をセットする。

### 4.4 エディタ UI

`TerrainEditorPanel::DrawGenerateTab()` に追加:

```
[Auto-Paint Splat] ボタン
  ├ Rock min altitude:   [slider 0.0 - 1.0]
  └ Rock slope threshold [slider 0 - 90 deg]
```

---

## 5. Phase 3 — 水面

### 5.1 目的

地形の水位（Sea Level）以下を覆う水面を追加する。  
水面は**地形とは別の平面メッシュ**として描画し、屈折・反射は後フェーズで追加する。

### 5.2 水面パラメータ

`TerrainAsset` に追加:

```cpp
struct WaterParams {
    bool   enabled       = false;
    float  seaLevel      = 0.0f;    // ワールドY座標
    float4 shallowColor  = {0.1f, 0.5f, 0.8f, 0.7f};  // RGBA
    float4 deepColor     = {0.0f, 0.1f, 0.4f, 0.9f};
    float  depthFade     = 5.0f;    // メートル単位の透明遷移深さ
    float  waveSpeed     = 0.5f;
    float  waveScale     = 0.02f;
};
WaterParams water;
```

### 5.3 新規ファイル一覧

| ファイル | 内容 |
|---------|------|
| `Source/Terrain/WaterRenderPass.h/.cpp` | IFrameGraphPass 実装 |
| `Data/Shader/WaterVS.hlsl/.cso` | 平面メッシュ + 波変位 |
| `Data/Shader/WaterPS.hlsl/.cso` | 深度フェード + 色ブレンド |

### 5.4 水面メッシュ

- 地形の worldSizeX × worldSizeZ と同じサイズの **16×16 格子平面**
- 頂点 Y = `water.seaLevel`（ビルド時生成、seaLevel 変更時にリビルド）
- TerrainBuildSystem が `WaterMeshBuffer` を生成・管理する
- `RenderQueue::waterDrawCall` として 1 回だけ登録

### 5.5 水面シェーダー (WaterPS.hlsl) 概要

```hlsl
// シーン深度から水深を計算
float sceneDepth  = gDepthTex.Sample(gPointClamp, screenUV);
float waterDepth  = LinearizeDepth(sceneDepth) - LinearizeDepth(input.posCS.z / input.posCS.w);

// 深度フェードで浅瀬/深海ブレンド
float t       = saturate(waterDepth / depthFade);
float4 color  = lerp(shallowColor, deepColor, t);

// 簡易波ノーマル (時間ベース UV スクロール 2方向)
float2 uv1    = input.uv + float2(time * waveSpeed, 0);
float2 uv2    = input.uv + float2(0, time * waveSpeed * 0.7);
float3 normal = normalize(gWaveNormal.Sample(gLinearWrap, uv1).rgb
              + gWaveNormal.Sample(gLinearWrap, uv2).rgb - 1.0);
// フレネル反射
float fresnel = pow(1.0 - saturate(dot(normal, viewDir)), 3.0);
color.rgb    += fresnel * 0.3;   // 簡易スカイ反射

return color;
```

### 5.6 EngineKernel — パス順序

```
DeferredLightingPass
TerrainRenderPass       ← 不透明地形
WaterRenderPass         ← 半透明水面 (地形の後)
SkyboxPass
```

### 5.7 エディタ UI

`TerrainEditorPanel::DrawSettingsTab()` に追加:

```
[Water]
  ✅ Enable Water
  Sea Level Y:   [slider -10 ~ 64]
  Shallow Color: [color picker]
  Deep Color:    [color picker]
  Depth Fade:    [slider 0.5 ~ 20]
  Wave Speed:    [slider 0.0 ~ 2.0]
```

---

## 6. Phase 4 — LOD

### 6.1 目的

`TerrainLODSystem` は現在スタブのみ。カメラ距離に応じてチャンクのインデックスバッファを切り替え、  
遠距離の三角形数を削減する。

### 6.2 LOD レベル定義

| LOD | 距離 (m) | 頂点間隔 | 備考 |
|-----|---------|---------|------|
| 0 | 0〜64 | 1倍 (フル解像度) | 近景 |
| 1 | 64〜128 | 2倍 | — |
| 2 | 128〜256 | 4倍 | — |
| 3 | 256〜512 | 8倍 | — |
| 4 | 512〜 | 16倍 | 遠景 |

### 6.3 データ構造変更

`TerrainChunkBuffer` に LOD バッファ配列を追加:

```cpp
struct TerrainChunkBuffer {
    // 既存
    XMFLOAT3 chunkWorldOffset;
    XMFLOAT3 boundsCenter;
    XMFLOAT3 boundsExtents;

    // LOD ごとの GPU バッファ
    struct LodMesh {
        std::unique_ptr<IBuffer> vertexBuffer;
        std::unique_ptr<IBuffer> indexBuffer;
        uint32_t                 indexCount = 0;
    };
    LodMesh lods[5];    // LOD 0〜4
    int     currentLod = 0;
};
```

### 6.4 TerrainLODSystem — 実装内容

```cpp
void TerrainLODSystem::Update(Registry& registry)
{
    // カメラ位置を取得 (RenderContext 経由 or CameraComponent)
    XMFLOAT3 camPos = GetCameraWorldPos(registry);

    for (auto [e, tc] : registry.View<TerrainComponent>()) {
        const TerrainRuntimeData* rd = m_buildSystem->GetRuntimeData(e);
        if (!rd) continue;
        for (auto& chunk : rd->chunks) {
            float dist = XMVectorGetX(XMVector3Length(
                XMLoadFloat3(&chunk.boundsCenter) - XMLoadFloat3(&camPos)));
            int lod = 0;
            for (int i = 0; i < 5; ++i) {
                if (dist > lodDistances[i]) lod = i + 1;
            }
            chunk.currentLod = std::min(lod, 4);
        }
    }
}
```

`TerrainExtractSystem::Extract()` では `chunk.lods[chunk.currentLod]` のバッファを draw call に登録する。

### 6.5 RebuildEntity の変更

各 LOD のインデックスバッファを別途生成する:

```cpp
// LOD i のステップ幅
int step = 1 << i;   // LOD0=1, LOD1=2, LOD2=4 ...
// 頂点は LOD0 共用 (メモリ節約)、インデックスのみ別生成
```

---

## 7. Phase 5 — エディタ強化

### 7.1 ブラシペイント実装 (現在未配線)

#### Scene View レイキャスト

EditorLayer のマウスクリックイベントで地形に対してレイキャストし、  
ヒット座標を `TerrainEditorPanel::ApplyBrush(worldX, worldZ)` に渡す。

実装箇所: `EditorLayer::OnMouseClick()` (新規追加) または `ImGuizmoBegin()` 付近

```cpp
// Scene View 内でクリック & ドラッグ中
if (ImGui::IsMouseDown(0) && isSceneViewHovered && m_showTerrainEditor) {
    Ray ray = ScreenToRay(mousePos, rc.view, rc.proj, viewportSize);
    float hitX, hitZ;
    if (RaycastTerrain(ray, selectedEntity, hitX, hitZ)) {
        m_terrainEditorPanel.ApplyBrush(registry, selectedEntity, hitX, hitZ);
    }
}
```

#### RaycastTerrain ユーティリティ

`TerrainBuildSystem` に追加:

```cpp
bool TerrainBuildSystem::Raycast(EntityID e, const Ray& ray,
                                  float& outX, float& outZ, float& outY);
```

粗いグリッドステップで高度サンプルと比較してヒット点を求める簡易実装。

### 7.2 レイヤーテクスチャ ドラッグ&ドロップ

`DrawLayersTab()` で各レイヤーの albedoPath / normalPath を  
Content Browser からドラッグ&ドロップで設定できるようにする。

### 7.3 スプラットマップ リアルタイムプレビュー

Paint タブで現在のスプラットマップをエディタ内 ImGui テクスチャとして表示する。  
(128×128 に縮小して表示)

---

## 8. データフロー全体図（発展後）

```
TerrainAsset (CPU)
  ├── heightData[]          → TerrainBuildSystem → GPU 頂点/インデックスバッファ (LOD0〜4)
  ├── splatData[]           → TerrainBuildSystem → GPU splatTexture (Texture2D<float4>)
  ├── layers[].albedoPath   → TextureManager      → GPU albedoTextures[4]
  ├── layers[].normalPath   → TextureManager      → GPU normalTextures[4]
  └── water.seaLevel        → TerrainBuildSystem → GPU WaterMeshBuffer

TerrainLODSystem.Update()   → chunk.currentLod を更新

TerrainExtractSystem.Extract()
  → RenderQueue::terrainChunks (各 chunk.currentLod のバッファ + テクスチャポインタ)
  → RenderQueue::waterDrawCall (有効時)

TerrainRenderPass.Execute()
  → DrawIndexed (各チャンク、4テクスチャ+スプラット)

WaterRenderPass.Execute()   (Phase 3 以降)
  → 半透明水面 DrawIndexed
```

---

## 9. テクスチャアセット一覧

### Phase 1 (既存ファイル、追加不要)

| ファイルパス | 内容 | 状態 |
|------------|------|------|
| `Data/Model/terrain/wispy-grass-meadow_albedo.png` | 草アルベド | ✅ 既存 |
| `Data/Model/terrain/rocky_dirt1-albedo.png` | 土/地面アルベド | ✅ 既存 |
| `Data/Model/terrain/layered-rock1-albedo.png` | 岩アルベド | ✅ 既存 |

### Phase 3+ (新規用意が必要)

| ファイルパス | 内容 | 形式 |
|------------|------|------|
| `Data/Model/terrain/wispy-grass-meadow_normal.png` | 草法線 | PNG Linear |
| `Data/Model/terrain/rocky_dirt1-normal.png` | 土法線 | PNG Linear |
| `Data/Model/terrain/layered-rock1-normal.png` | 岩法線 | PNG Linear |
| `Data/Model/terrain/water_normal.png` | 波法線 (アニメ用) | PNG Linear |

法線マップは Poly Haven / AmbientCG から対応するマテリアルの法線をダウンロードして配置する。

---

## 10. 実装順の推奨

```
Phase 1: スプラットシェーダー (最優先)
  └ 効果が目に見えて確認できる。テクスチャを1枚置けばすぐに動く。

Phase 2: 自動スプラット生成
  └ Phase 1 が動いていれば即座に見栄えが大幅改善する。

Phase 3: 水面
  └ ゲームとして最も「それらしい」見た目に直結する。

Phase 4: LOD
  └ 最適化。遠距離地形で FPS が落ちてから実装しても遅くない。

Phase 5: エディタ強化
  └ ブラシが使えないと不便だが、自動スプラット(Phase 2)があれば当面代替できる。
```

---

## 11. 制約・注意事項

- **テクスチャスロット数**: DX12 のデスクリプタヒープのバインド数に注意。  
  現在の ICommandList が `PSSetTexture(slot, ...)` で何スロットまで対応しているか確認すること。
- **スプラットテクスチャの更新**: `splatData` を CPU で変更した場合、GPU テクスチャを再アップロードする必要がある。頻繁なブラシ操作では UpdateBuffer の頻度に注意。
- **水面と深度バッファ**: 水面パスは SceneColor に半透明合成するため、先に地形の不透明パスで深度バッファを書き込んでおく必要がある（現行のパス順序で問題ない）。
- **LOD のモーフィング**: LOD 切り替え時のポッピング(境界線のチラつき)は Phase 4 MVP では許容し、モーフィングは Phase 6 以降で検討する。

---

*この仕様書は実装進行に合わせて随時更新する。*
