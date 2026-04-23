# メッシュパーティクル SoA 仕様書 (A-full 圧縮版)

作成日: 2026-04-21
対象: DX12 GPU-driven Effect Runtime (SoA v3) + EffectEditor
目的: **壊れているメッシュ粒子パスを再設計し、拡張性を残しつつエフェクトに必要な機能に厳選して稼働させる**

---

## 0. 背景と現状診断

### 0.1 前提
既存 GPU パイプライン (`EffectParticlePass.cpp`) は SoA v3 で書き直されており、Billboard/Ribbon は 88B/粒子の SoA ストリーム (`BillboardHot` 32B + `BillboardWarm` 16B + `BillboardCold` 32B + `BillboardHeader` 8B) 上で稼働中。

### 0.2 壊れている点 (確認済み)
`EffectParticleMeshVS.hlsl` が期待するのは旧 AoS `particle_data`（`Shader/compute_particle.hlsli` 定義、数百バイトの大型構造体）だが、CPU (`EffectParticlePass.cpp:2869`) は SoA の `BillboardHot` (32B) と `BillboardHeader` (8B) をバインドしている。

- **ストライド不一致** → `particle_data_buffer[index]` が誤ったメモリを読む
- **ヘッダ構造不一致** → `header.alive` / `header.particle_index` が実体と別の位置を指す
- **SoA には回転 quaternion と 3 軸スケールが無い** → VS を SoA 対応に書き換えても必要情報が足りない

結論: **シェーダ・CPU バインディング・データスキーマの 3 層すべてに穴があり、authoring UI を直しても描画されない**。

---

## 1. スコープ（厳選・拡張性維持）

### 1.1 実装する機能 (V1)
| 機能 | 根拠 |
|---|---|
| 位置・速度 | 既存 Hot 流用 |
| 3 軸スケール（emit 時定数） | メッシュ形状の伸縮に必須 |
| クォータニオン回転（毎フレーム積分） | 破片・剣閃・矢の向きに必須 |
| 固定軸角速度（emit 時定数） | 回転する破片・手裏剣型エフェクトに必須 |
| 頂点カラー × 粒子色 Tint | Warm 流用 |
| ライフタイム Fade | Hot の age/life 流用 |
| Additive / PremultipliedAlpha ブレンド | 既存 PSO 流用 |
| 1 エミッタ = 1 メッシュ（固定） | 現状 packet は single modelResource |

### 1.2 V1 で省く (拡張点は残す)
| 機能 | 省略理由 | 拡張フック |
|---|---|---|
| スキニング（ボーン） | 粒子個体へのアニメは稀、VS 分岐が重い | MeshAttribStream に 16B `_reserved` を確保 |
| 粒子ごとのメッシュ変種 | packet 設計変更が必要 | 同上 `_reserved` + packet 拡張余地 |
| Velocity Stretch | 非 billboard 用の歪み表現は別機能 | - |
| 法線マップ / PBR | まず形状が出ることを優先 | PS を将来 Uber 化 |
| 粒子ごとの角加速度ランダム変化 | cbuffer + hash で代替可能 | MeshAttribStream に 4B `_rot_reserved` |

---

## 2. データレイアウト

### 2.1 新規追加: `MeshAttribStream` (64B / 粒子)
`Shader/EffectParticleSoA.hlsli` に追記:

```hlsl
// Mesh Attribute Stream (64 bytes) — quat rotation + 3-axis scale + angular velocity
// Referenced only when particle is in Mesh renderer bin.
struct MeshAttribHot
{
    float4 rotation;        // 16B quaternion (current orientation, updated each frame)
    float3 scale;            // 12B per-axis scale (set at emit, static unless later animated)
    float  angularSpeed;     //  4B radians/sec around angularAxis (0 = no spin)
    float3 angularAxis;      // 12B normalized axis (set at emit)
    float  _rotReserved;     //  4B (future: rotation damping)
    uint4  _reserved;        // 16B (future: skinning handle, variant idx, material flags, ...)
};
static const uint MESH_ATTRIB_STRIDE = 64;
```

**サイズ選択理由**: 64B は GPU L1 キャッシュラインに載るサイズ。拡張用 20B を `_reserved` 群に確保。

### 2.2 既存ストリームは共有
メッシュ粒子でも Billboard 系と共有:
- `BillboardHot` (32B): position / ageLifePacked / velocity / sizeSpin
  - `sizeSpin.x` (currentSize) → メッシュでは **等比スケール補正**（MeshAttribHot.scale と乗算 = final scale）
  - `sizeSpin.y` (spinAngle) → メッシュでは **未使用**（mesh は独自の quaternion を使う）
- `BillboardWarm` (16B): packedColor / packedEndColor / texcoordPacked / flags → そのまま
- `BillboardHeader` (8B): slotIndex / packed → そのまま

**SoA 一貫性の利点**: ScatterAlive / BuildBins / CoarseDepth など既存の bin/page 管理ロジックに一切手を入れない。

### 2.3 アリーナバッファ追加
`ParticleSharedArenaBuffers` (`EffectParticlePass.cpp:249-275`) に以下を追加:

```cpp
std::unique_ptr<IBuffer> meshAttribHotBuffer;
D3D12_RESOURCE_STATES meshAttribHotState = D3D12_RESOURCE_STATE_COMMON;
```

同じ `totalCapacity` で allocate（全粒子が mesh に成れる前提の単純戦略。必要なら将来 MeshMaxCapacity を分離）。

---

## 3. コンピュート経路

### 3.1 Emit (`EffectParticleEmit_cs.hlsl`)
追加入力バインド:
```hlsl
RWStructuredBuffer<MeshAttribHot> g_MeshAttribHot : register(u8);
```

cbuffer (`EffectParticleRuntimeCommon.hlsli` 側) に emit 時定数を追加:
```hlsl
float4 gMeshInitialScale;          // xyz = scale, w = scaleRandomRange
float4 gMeshAngularAxisSpeed;      // xyz = axis (normalized), w = angularSpeed (rad/s)
float4 gMeshAngularRandomOrient;   // x = random initial yaw range, y = pitch range, z = roll range, w = random speed factor
uint   gIsMeshMode;                // 0 = billboard/ribbon, 1 = mesh (ゼロなら下記ブロックをスキップ)
```

CSMain 末尾（dead 粒子書込み完了後）に追記:
```hlsl
if (gIsMeshMode != 0u)
{
    float3 scale = gMeshInitialScale.xyz
        * lerp(1.0f - gMeshInitialScale.w, 1.0f + gMeshInitialScale.w, Hash01(seed * 2711u + slot));

    float yaw   = lerp(-gMeshAngularRandomOrient.x, gMeshAngularRandomOrient.x, Hash01(seed * 3331u + slot * 11u));
    float pitch = lerp(-gMeshAngularRandomOrient.y, gMeshAngularRandomOrient.y, Hash01(seed * 4447u + slot * 13u));
    float roll  = lerp(-gMeshAngularRandomOrient.z, gMeshAngularRandomOrient.z, Hash01(seed * 5557u + slot * 17u));
    float4 q = QuatFromYawPitchRoll(yaw, pitch, roll);  // 新規 util 関数

    float speedRand = lerp(1.0f - gMeshAngularRandomOrient.w, 1.0f + gMeshAngularRandomOrient.w, Hash01(seed * 6673u + slot * 19u));

    MeshAttribHot m;
    m.rotation      = q;
    m.scale         = scale;
    m.angularSpeed  = gMeshAngularAxisSpeed.w * speedRand;
    m.angularAxis   = normalize(gMeshAngularAxisSpeed.xyz);
    m._rotReserved  = 0.0f;
    m._reserved     = uint4(0,0,0,0);
    g_MeshAttribHot[slot] = m;
}
```

### 3.2 Update (`EffectParticleUpdate_cs.hlsl`)
追加バインド:
```hlsl
RWStructuredBuffer<MeshAttribHot> g_MeshAttribHot : register(u8);
```

位置/速度更新の後に追記（**全粒子に対してではなく**、mesh renderer bin に属する粒子のみ、つまり `gIsMeshMode != 0` のエミッタから来たパスでのみ実行）:
```hlsl
if (gIsMeshMode != 0u && HeaderIsAlive(packed))
{
    MeshAttribHot m = g_MeshAttribHot[slot];
    float4 dq = QuatFromAxisAngle(m.angularAxis, m.angularSpeed * gTiming.x);  // gTiming.x = dt
    m.rotation = QuatNormalize(QuatMultiply(dq, m.rotation));
    g_MeshAttribHot[slot] = m;
}
```

Quaternion ヘルパは `EffectParticleSoA.hlsli` に集約:
```hlsl
float4 QuatFromAxisAngle(float3 axis, float angle) { ... }
float4 QuatMultiply(float4 a, float4 b) { ... }
float4 QuatNormalize(float4 q) { ... }
float4 QuatFromYawPitchRoll(float y, float p, float r) { ... }
float3 QuatRotate(float3 v, float4 q) { ... }
```

### 3.3 Compute Root Signature 変更
`simulationRootSignature` の root parameter に u8 を追加（mesh 不使用時も NullDescriptor 許容）。

---

## 4. 描画経路

### 4.1 シェーダ全書き直し: `EffectParticleMeshVS.hlsl`

```hlsl
#include "EffectParticleSoA.hlsli"

StructuredBuffer<BillboardHot>     g_Hot       : register(t0);
StructuredBuffer<BillboardWarm>    g_Warm      : register(t1);
StructuredBuffer<BillboardHeader>  g_Header    : register(t2);
StructuredBuffer<MeshAttribHot>    g_MeshAttr  : register(t3);

cbuffer CbScene : register(b0) { row_major float4x4 viewProjection; /* ... */ };
cbuffer CbRender: register(b2) { float global_alpha; /* ... */ };

struct VS_IN
{
    float3 pos : POSITION;
    float4 boneWeights : BONE_WEIGHTS;     // V1 は未使用、入力レイアウト互換のため受取
    uint4  boneIndices : BONE_INDICES;
    float2 uv  : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 color : COLOR0;
};

struct VS_OUT { float4 position : SV_POSITION; float2 uv : TEXCOORD0; float4 color : COLOR0; float3 normal : TEXCOORD1; };

VS_OUT main(VS_IN input, uint instanceID : SV_InstanceID)
{
    VS_OUT output;
    BillboardHeader hdr = g_Header[instanceID];
    uint slot = hdr.slotIndex;
    float aliveFlag = HeaderIsAlive(hdr.packed) ? 1.0f : 0.0f;

    BillboardHot  hot  = g_Hot[slot];
    BillboardWarm warm = g_Warm[slot];
    MeshAttribHot m    = g_MeshAttr[slot];

    float2 sizeSpin = UnpackHalf2(hot.sizeSpin);
    float3 scale    = m.scale * sizeSpin.x;           // 3-axis base × uniform size factor
    float3 localPos = input.pos * scale * aliveFlag;

    float3 worldPos    = QuatRotate(localPos, m.rotation) + hot.position;
    float3 worldNormal = normalize(QuatRotate(input.normal, m.rotation));

    output.position = mul(float4(worldPos, 1.0f), viewProjection);
    output.uv       = input.uv;
    output.normal   = worldNormal;
    output.color    = input.color * UnpackRGBA8(warm.packedColor);
    output.color.a *= global_alpha;
    return output;
}
```

### 4.2 `EffectParticleMeshPS.hlsl`
V1 はシンプル tint + texture sample のまま（変更最小）。ただし VS_OUT 構造に整合させる。

### 4.3 メッシュ描画 Root Signature 拡張
`EffectParticlePass.cpp:1804-1820` の params 配列を 6 要素化:

| index | 種別 | register | 用途 |
|---|---|---|---|
| 0 | CBV | b0 | Scene |
| 1 | SRV | t0 | BillboardHot |
| 2 | SRV | t1 | BillboardWarm |
| 3 | SRV | t2 | BillboardHeader |
| 4 | SRV | t3 | MeshAttribHot |
| 5 | CBV | b2 | Render |
| 6 | table | t4 | Texture |

静的サンプラは s1 へずらす（既存通り）。

### 4.4 `MeshDrawEntry` 拡張
```cpp
struct MeshDrawEntry {
    const EffectParticlePacket* packet = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS hotGpuVa         = 0ull;
    D3D12_GPU_VIRTUAL_ADDRESS warmGpuVa        = 0ull;
    D3D12_GPU_VIRTUAL_ADDRESS headerGpuVa      = 0ull;
    D3D12_GPU_VIRTUAL_ADDRESS meshAttribGpuVa  = 0ull;   // 新規
    DX12Buffer* indirectArgsBuffer = nullptr;
    uint32_t drawCount = 0;
};
```

`EffectParticlePass.cpp:2869` の push_back、および `:3056-3088` の描画ループを 4 SRV バインドに更新。

### 4.5 粒子数 → instance count
現状 `drawCount` は CPU 側 readback 値で、readback は throttled。V1 はこのまま（billboard と同じ精度）。将来 ExecuteIndirect 対応に置き換え可能。

---

## 5. オーサリング経路

### 5.1 EffectGraphAsset (`EffectParticleSimulationLayout`)
既存の下記フィールドを実際に使う:
- `EffectParticleDrawMode drawMode` — `Mesh` を compile 段階で確定
- `std::string meshAssetPath` — `EffectParticleMeshRenderer` 的なノードの `stringValue2` から読む

追加フィールド (compiled asset):
```cpp
DirectX::XMFLOAT3 meshInitialScale   = { 1, 1, 1 };
float             meshScaleRandom    = 0.0f;
DirectX::XMFLOAT3 meshAngularAxis    = { 0, 1, 0 };
float             meshAngularSpeed   = 0.0f;        // rad/s
DirectX::XMFLOAT3 meshAngularOrientRandom = { 0, 0, 0 };  // yaw/pitch/roll の ±範囲 (rad)
float             meshAngularSpeedRandom  = 0.0f;   // 速度係数 ±範囲
```

### 5.2 Compiler (`EffectCompiler.cpp`)
現行 L411-413 のハードコード除去:
```cpp
// before
if (drawMode == Mesh) { compiled->particleRenderer.meshAssetPath = asset.previewDefaults.previewMeshPath; }
```
```cpp
// after
if (drawMode == Mesh) {
    compiled->particleRenderer.meshAssetPath = !node->stringValue2.empty()
        ? node->stringValue2
        : asset.previewDefaults.previewMeshPath;
    compiled->particleRenderer.meshInitialScale         = node->vec3A;  // フィールド拡張
    compiled->particleRenderer.meshScaleRandom          = node->floatA;
    compiled->particleRenderer.meshAngularAxis          = node->vec3B;
    compiled->particleRenderer.meshAngularSpeed         = node->floatB;
    compiled->particleRenderer.meshAngularOrientRandom  = node->vec3C;
    compiled->particleRenderer.meshAngularSpeedRandom   = node->floatC;
}
```

※ `vec3A/B/C` と `floatA/B/C` は EffectGraphNode に追加するフィールド。既存 `intValue` 等の命名を踏襲。

### 5.3 EffectGraphNode (`EffectGraphAsset.h`)
```cpp
std::string stringValue2;                      // mesh asset path
DirectX::XMFLOAT3 vec3A { 1, 1, 1 };           // meshInitialScale
DirectX::XMFLOAT3 vec3B { 0, 1, 0 };           // meshAngularAxis
DirectX::XMFLOAT3 vec3C { 0, 0, 0 };           // meshAngularOrientRandom
float floatA = 0.0f;                           // meshScaleRandom
float floatB = 0.0f;                           // meshAngularSpeed
float floatC = 0.0f;                           // meshAngularSpeedRandom
```

将来 mesh 以外のノードでも流用できる汎用フィールドとして名前は generic に。JSON シリアライズに各フィールドを追加、存在しない場合は default 値で吸収（後方互換）。`schemaVersion` を `+1`。

### 5.4 EffectEditor UI
`EffectEditorPanel.cpp:3370+` (Node mode) と `:2063+` (Stack mode) の SpriteRenderer Inspector に、`drawMode == Mesh` のときだけ以下を表示:

```
[Mesh Asset]        <asset picker, stringValue2>
[Initial Scale]     <XYZ DragFloat3, vec3A>   [± Random] <DragFloat, floatA>
[Angular Axis]      <XYZ DragFloat3, vec3B>
[Angular Speed]     <DragFloat rad/s, floatB> [± Random] <DragFloat, floatC>
[Initial Orient Random YPR] <DragFloat3, vec3C>
```

`drawMode == Billboard/Ribbon` のときはテクスチャ欄のみ（既存）。

### 5.5 プレビュー反映
Editor の preview entity 生成パス (`EffectEditorPanel.cpp::QueuePreviewSpawn*`) は compiled asset を使うので、4 章と 5 章が揃えば自動で動く。

---

## 6. 実装フェーズ分割

### Phase 1a: 既存無回帰の土台整備（1〜1.5h）
**完了条件**: MeshAttribHot バッファ / cbuffer 拡張 / u8 常時バインドが入っても、既存 Billboard/Ribbon が従来通り描画される
- 1a.1 `EffectParticleSoA.hlsli` に MeshAttribHot 構造体と Quat helpers を**定義のみ**追記（まだ誰も読まない）
- 1a.2 `EffectParticleRuntimeCommon.hlsli` の cbuffer に `gMeshInitialScale` / `gMeshAngularAxisSpeed` / `gMeshAngularRandomOrient` / `gIsMeshMode` を追記
- 1a.3 CPU 側 twin 構造体を**同一コミット内で同サイズに更新**、既存 emit 呼び出しは mesh フィールドを 0 で埋める
- 1a.4 `ParticleSharedArenaBuffers::meshAttribHotBuffer` 追加、allocate / Transition
- 1a.5 `simulationRootSignature` に u8 UAV を追加、全 compute Dispatch で `meshAttribHotBuffer` を u8 に常時バインド
- **検証 (Phase 1a 完了条件)**:
  - Billboard プリセット 3 件プレビュー → スクショ差分ゼロ
  - Ribbon プリセット 1 件プレビュー → スクショ差分ゼロ
  - DRED ログに新規エラー無し
  - PIX 計測で billboard のみ稼働時、フレーム時間差が ±2% 以内
  - 既存 .effectgraph JSON（schemaVersion 旧）を再保存せずロード、drawMode / 粒子数 / duration が一致

### Phase 1b: メッシュ描画本体（3〜3.5h）
**完了条件**: マニュアルテストアセット 1 件で quaternion 回転しながらメッシュ粒子が画面に出る
- 1b.1 Emit shader の `gIsMeshMode != 0` 分岐で MeshAttribHot 書込み
- 1b.2 Update shader で角速度積分（quat composition + normalize）
- 1b.3 Mesh VS/PS 全書き直し、root sig 7-param 化（b0 + t0..t3 + b2 + table t4）、PSO 再生成
- 1b.4 `MeshDrawEntry` 拡張（hot/warm/header/meshAttrib の 4 GPU VA 保持）、draw loop 4-SRV binding
- 1b.5 直書きテストパケット (drawMode=Mesh + 固定 mesh + 固定 scale/angular) で画面描画確認

### Phase 2: オーサリング接続（半日）
**完了条件**: EffectEditor で Mesh 粒子が作成→プレビューできる
- 2.1 `EffectGraphNode` のフィールド追加 + JSON I/O + schemaVersion bump
- 2.2 `EffectCompiler` のハードコード除去 + 新パラメータ反映
- 2.3 `EffectParticleSimulationLayout` / `CompiledEffectAsset` 拡張
- 2.4 Stack / Node モード両方の Inspector UI 追加
- 2.5 既存 .effectgraph asset の互換性確認（読み込んで壊れないか）

### Phase 3: 仕上げ（2〜3時間）
- 3.1 テンプレート: **Debris (Mesh)** / **Spinning Shards** の 2 プリセット追加
- 3.2 Stop/Pause/Scrub との同時検証（前タスクで直した Timeline と噛み合うか）
- 3.3 PIX で描画呼び出し確認（余分な DrawIndexedInstanced が走っていないか）
- 3.4 Docs update (本仕様書の検証結果セクション追記)

### 総工数見積 (気合込み)
| Phase | 見積時間 | 見積トークン |
|---|---|---|
| 1a | 1〜1.5h | 50〜70k |
| 1b | 3〜3.5h | 120〜150k |
| 2 | 3〜4h | 100〜150k |
| 3 | 2〜3h | 80〜120k |
| **合計** | **9〜12h** | **350〜490k** |

前回見積 (2〜3日) からの短縮根拠:
- SoA ストリームを**新規追加するだけ**で既存 Emit/Update/ScatterAlive/Bin/Page ロジックを改変しない
- MeshAttribHot は u8 に配置して他と衝突しない
- Authoring は既存 DrawAssetSlotControl + 既存 compiler 関数で大半再利用
- V1 スコープで skin / variant / Velocity Stretch を外す

---

## 7. リスク & 緩和

| リスク | 緩和策 |
|---|---|
| Root sig 変更で既存 billboard パスが壊れる | billboard と mesh は**別 root sig** 系統なので独立。PSO 再ロード範囲は mesh のみ |
| Emit/Update の u8 バインドが全エミッタで必須になる | `gIsMeshMode == 0` で分岐スキップ、D3D 側は NullDescriptor でも合法 |
| MeshAttribHot 64B × 全粒子分で VRAM 増 | 既定 MAX_PARTICLES=65536 → +4MB 。無視できる |
| Quat 正規化の数値誤差でメッシュが歪む | Update 毎回 `QuatNormalize` を通す（上記 3.2 の例示済み） |
| 既存 .effectgraph ファイルの読み込み壊れ | schemaVersion bump + 欠落フィールドは default 値吸収、Load テスト通過を Phase 2.5 で実施 |
| PIX / DRED で古い mesh VS の DX 破損ログが残っている | Phase 1 完了時点で DRED ログを空にして再検証 |

---

## 8. 非目標（明示的に扱わない）

- 粒子 LOD
- CPU 側スポーナ（CPU particle）
- メッシュパーティクルの GPU ソーティング（SoA bin は使わず描画順は emitter ごと、V1 は additive 前提）
- Indirect ExecuteIndirect による drawCount 最新化（readback 粒度そのまま）

これらは将来別仕様書で扱う。

---

## 9. 完了判定チェックリスト

- [ ] Phase 1 手動テストで 1 個の mesh 粒子が quaternion 回転しながらフェードアウトする
- [ ] Phase 1 で 1000 粒子同時出しても GPU timeout しない（PIX で frame < 16ms）
- [ ] Phase 2 で新規 Mesh エフェクトを Editor で作成 → 保存 → 再ロード → プレビュー表示 まで往復成功
- [ ] Phase 2 で既存 .effectgraph アセット（Billboard 系）のロードが壊れない
- [ ] Phase 3 テンプレ 2 種が Template メニューから生成 → 即プレビュー描画
- [ ] DRED ログに新規エラーが無い
- [ ] 本仕様書「9. 完了判定チェックリスト」が全て埋まる

---

## 10. 疑問点（実装前に確認したい）

1. `meshAngularOrientRandom` の単位は **ラジアン** で統一する（UI 側でも rad 表示 or DegreesToRadians 内部変換どちらを好むか？）
2. `meshInitialScale.w (random)` は **全軸一様**にランダム化するか、**軸別**にするか（UI の複雑度と見合い）
3. 将来のスキニングを本当に残したいか（残さないなら VS_IN から BONE_WEIGHTS/INDICES を削って input layout を軽量化可能）

実装着手前に上記 3 点を確定できれば理想。未確定でも default (rad、全軸一様、スキニング枠は残す) で進められます。
