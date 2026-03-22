# GPU-Driven Indirect Draw & Compute Culling 完全仕様書

**改訂**: v2.0 — 2Dディスパッチ、ワールド空間バウンズ、スケール対応

## 1. 目的

DX12環境でExecuteIndirectによる間接描画を基本描画パスとして完成させ、
その上にCompute Shaderフラスタムカリングを載せる。
DX11互換は維持する。保守性・メンテナンス性を最優先とする。

---

## 2. 現行設計の問題点

### 2.1 バッチ粒度の不一致 (致命的)
- **BuildIndirectCommandPass**: 1バッチ × Nメッシュ → N個のDrawArgs生成
- **ComputeCullingPass**: 1バッチ → 1個のGpuDrawArgs生成
- マルチメッシュモデルで描画引数が足りなくなる

### 2.2 HLSL の線形探索
- globalIdx からコマンド所属を線形探索 → O(N×C) で非スケーラブル
- commandCount が少ない前提に依存

### 2.3 バウンズ計算の不正
- ローカル空間の原点位置 (`_41,_42,_43`) をバウンズ中心として使用
- モデルのローカルバウンズ中心がワールド変換されていない
- スケールがバウンディング半径に反映されていない

### 2.4 RHI抽象化の破れ
- ComputeCullingPass内でraw D3D12 API (`CreateCommittedResource`, `Map`, `Unmap`) を直接使用
- `m_drawArgsStagingBuffer` が `ComPtr<ID3D12Resource>` でIBuffer経由でない

### 2.5 バッファ寿命の曖昧さ
- `gpuCulledInstanceBuffer` が ComputeCullingPass のメンバ AND RenderContext の shared_ptr に重複
- どちらが所有者か不明

### 2.6 エラーハンドリング不足
- StagingバッファのMap失敗→nullptrでmemcpy→クラッシュ
- UAVバッファ作成失敗時のフォールバックなし

### 2.7 マジックナンバー
- `drawArgsIndex * 20 + 4` のようなオフセット計算がHLSL/C++に散在

---

## 3. 共通定数・構造体定義

全てのPass/Shader/HLSLで共有するヘッダに定義する。

### 3.1 共通定数
```cpp
// IndirectDrawCommon.h
static constexpr uint32_t DRAW_ARGS_STRIDE          = 20;  // sizeof(DrawArgs)
static constexpr uint32_t DRAW_ARGS_INSTANCE_COUNT_OFFSET = 4;  // instanceCountフィールドのバイトオフセット
static constexpr uint32_t INSTANCE_DATA_STRIDE       = 128; // sizeof(InstanceData)
static constexpr uint32_t CULL_THREAD_GROUP_SIZE     = 64;
```

HLSL側:
```hlsl
// IndirectDrawCommon.hlsli
#define DRAW_ARGS_STRIDE               20
#define DRAW_ARGS_INSTANCE_COUNT_OFFSET 4
#define INSTANCE_DATA_STRIDE           128
#define CULL_THREAD_GROUP_SIZE         64
```

### 3.2 DrawArgs (20 bytes)
```cpp
struct DrawArgs {
    uint32_t indexCountPerInstance;
    uint32_t instanceCount;
    uint32_t startIndexLocation;
    int32_t  baseVertexLocation;
    uint32_t startInstanceLocation;
};
static_assert(sizeof(DrawArgs) == DRAW_ARGS_STRIDE);
```
- `D3D12_DRAW_INDEXED_ARGUMENTS` とバイナリ互換
- CPU書込み(UPLOAD) / GPU書込み(UAV) 両方で同一レイアウト

### 3.3 InstanceData (128 bytes)
```cpp
struct InstanceData {
    XMFLOAT4X4 worldMatrix;       // 64 bytes
    XMFLOAT4X4 prevWorldMatrix;   // 64 bytes
};
static_assert(sizeof(InstanceData) == INSTANCE_DATA_STRIDE);
```

### 3.4 IndirectDrawCommand (CPU側メタデータ)
```cpp
struct IndirectDrawCommand {
    DrawBatchKey              key;
    std::shared_ptr<ModelResource> modelResource;
    uint32_t meshIndex;            // モデル内メッシュ番号
    uint32_t drawArgsIndex;        // DrawArgsバッファ内のインデックス
    uint32_t firstInstance;        // インスタンスバッファ内の開始位置
    uint32_t instanceCount;        // インスタンス数
    bool     supportsInstancing;   // false=スキンドメッシュ
};
```

---

## 4. バッファ設計

### 4.1 所有権テーブル

| 所有者 | バッファ | BufferType | ヒープ | 寿命 |
|---|---|---|---|---|
| RenderContext | preparedInstanceBuffer | Vertex | UPLOAD | フレーム再利用 |
| RenderContext | preparedDrawArgsBuffer | Indirect | UPLOAD | フレーム再利用 |
| ComputeCullingPass | m_culledInstanceBuffer | UAVStorage | DEFAULT | Pass寿命 |
| ComputeCullingPass | m_culledDrawArgsBuffer | UAVStorage | DEFAULT | Pass寿命 |
| ComputeCullingPass | m_stagingBuffer | Vertex(UPLOAD) | UPLOAD | Pass寿命 |
| ComputeCullingPass | m_cullMetaBuffer | Vertex(UPLOAD) | UPLOAD | Pass寿命 |

### 4.2 所有権ルール
- ComputeCullingPassが所有するバッファはPassのメンバ変数に持つ
- RenderContextには **生ポインタ** (`IBuffer*`) のみ設定（所有権なし）
- フレーム毎に生ポインタをリセット、バッファ実体はPass内で容量不足時のみ再作成
- 全バッファ作成はIResourceFactory経由（RHI抽象化維持）

### 4.3 RenderContext インターフェース

```cpp
struct RenderContext {
    // === 描画パスが参照するフィールド (Renderer向け) ===
    IBuffer*  activeInstanceBuffer  = nullptr;   // VB slot1
    uint32_t  activeInstanceStride  = INSTANCE_DATA_STRIDE;
    IBuffer*  activeDrawArgsBuffer  = nullptr;   // ExecuteIndirect引数
    std::vector<IndirectDrawCommand> activeDrawCommands;
    std::vector<IndirectDrawCommand> activeSkinnedCommands;

    // === 内部管理 (Pass間受け渡し、Rendererは不使用) ===
    std::vector<InstanceData>         preparedInstanceData;
    std::shared_ptr<IBuffer>          preparedInstanceBuffer;
    std::shared_ptr<IBuffer>          preparedDrawArgsBuffer;
    uint32_t preparedInstanceCapacity = 0;
    uint32_t preparedDrawArgsCapacity = 0;
    uint32_t preparedVisibleInstanceCount = 0;
    std::vector<PreparedInstanceBatch> preparedOpaqueInstanceBatches;
};
```

**設計原則**: ModelRendererは `active*` フィールドだけを見る。
CPU/GPU cullingの選択はPassが吸収し、Rendererには漏れない。

---

## 5. パイプライン実行フロー

```
ExtractVisibleInstancesPass
  CPU: エンティティ→バッチ抽出
  出力: visibleOpaqueInstanceBatches
       ↓
BuildInstanceBufferPass
  CPU→GPU: 全インスタンスをVBに書込み
  設定: rc.activeInstanceBuffer = preparedInstanceBuffer
       ↓
BuildIndirectCommandPass
  CPU→GPU: 1バッチ×Nメッシュ → N個のDrawArgs
  設定: rc.activeDrawArgsBuffer = preparedDrawArgsBuffer
        rc.activeDrawCommands / activeSkinnedCommands
       ↓
ComputeCullingPass (DX12のみ, オプション)
  GPU: 2Dディスパッチでフラスタムカリング
  上書き: rc.activeInstanceBuffer = m_culledInstanceBuffer
          rc.activeDrawArgsBuffer = m_culledDrawArgsBuffer
          rc.activeDrawCommands (drawArgsIndex更新)
  復帰: GlobalRootSignature::BindAll()
       ↓
ShadowPass → GBufferPass → DeferredLighting → ...
  全て rc.active* を参照して描画
```

---

## 6. Compute Culling 詳細仕様

### 6.1 2Dディスパッチ方式

**線形探索を完全に排除する。**

```cpp
// CPU側ディスパッチ
uint32_t maxInstancesPerCmd = 0;
for (auto& cmd : cullCommands)
    maxInstancesPerCmd = std::max(maxInstancesPerCmd, cmd.instanceCount);

uint32_t groupsX = (maxInstancesPerCmd + CULL_THREAD_GROUP_SIZE - 1) / CULL_THREAD_GROUP_SIZE;
uint32_t groupsY = commandCount;
Dispatch(groupsX, groupsY, 1);
```

- **X軸**: コマンド内のインスタンスインデックス
- **Y軸**: コマンドインデックス
- スレッドは `SV_GroupID.y` で自身のコマンドを即座に知る
- コマンド数に依存しない O(1) ルックアップ

### 6.2 Compute Root Signature
```
Slot 0: b0 CBV  : CullingParams
Slot 1: t0 SRV  : InstanceData[]           (入力)
Slot 2: t1 SRV  : CullCommandMeta[]        (入力)
Slot 3: u0 UAV  : InstanceData[]           (出力: カリング後)
Slot 4: u1 UAV  : DrawArgs[] (RWByteAddress) (出力: atomic更新)
```

### 6.3 CullingParams (CBV)
```cpp
struct CullingParams {
    XMFLOAT4 frustumPlanes[6];   // 96 bytes
    uint32_t commandCount;        // 4 bytes
    uint32_t maxInstancesPerCmd;  // 4 bytes (X軸早期リターン用)
    uint32_t pad[2];              // 8 bytes → 112 bytes (256境界はCBリングが保証)
};
```

### 6.4 CullCommandMeta (SRV入力, per-command)
```cpp
struct CullCommandMeta {
    // --- インスタンス範囲 ---
    uint32_t firstInstance;        // 入力バッファ内開始位置
    uint32_t instanceCount;        // このコマンドのインスタンス数

    // --- 出力先 ---
    uint32_t outputInstanceStart;  // 出力インスタンスバッファの書込み開始位置
    uint32_t drawArgsIndex;        // DrawArgsバッファ内のインデックス (別フィールド)

    // --- メッシュ情報 (DrawArgs初期値に使用) ---
    uint32_t indexCount;           // メッシュのインデックス数
    int32_t  baseVertex;           // ベース頂点オフセット

    // --- バウンズ (ローカル空間) ---
    float    boundsCenterX;        // ローカルAABB中心X
    float    boundsCenterY;        // ローカルAABB中心Y
    float    boundsCenterZ;        // ローカルAABB中心Z
    float    boundsRadius;         // ローカルバウンディング球半径

    uint32_t pad[2];               // → 48 bytes, 16-byte aligned
};
static_assert(sizeof(CullCommandMeta) == 48);
```

**outputInstanceStart vs drawArgsIndex**:
- `outputInstanceStart`: 出力InstanceData[]内のオフセット。各コマンドの書込み領域の先頭。
- `drawArgsIndex`: DrawArgs[]内のインデックス。atomic incrementでinstanceCountを更新する対象。
- 1:1対応だが意味が異なるため別フィールドで管理。

### 6.5 HLSL (FrustumCullCS.hlsl)

```hlsl
#include "IndirectDrawCommon.hlsli"

cbuffer CullingParams : register(b0) {
    float4 frustumPlanes[6];
    uint   commandCount;
    uint   maxInstancesPerCmd;
    uint2  pad;
};

struct InstanceData {
    float4x4 worldMatrix;
    float4x4 prevWorldMatrix;
};

struct CullCommandMeta {
    uint  firstInstance;
    uint  instanceCount;
    uint  outputInstanceStart;
    uint  drawArgsIndex;
    uint  indexCount;
    int   baseVertex;
    float boundsCenterX, boundsCenterY, boundsCenterZ;
    float boundsRadius;
    uint2 pad;
};

StructuredBuffer<InstanceData>    inputInstances  : register(t0);
StructuredBuffer<CullCommandMeta> commands        : register(t1);
RWStructuredBuffer<InstanceData>  outputInstances : register(u0);
RWByteAddressBuffer               drawArgsBuffer  : register(u1);

// ─── ワールド空間バウンズ変換 ───
void TransformBounds(
    float3 localCenter, float localRadius,
    float4x4 world,
    out float3 worldCenter, out float worldRadius)
{
    // ローカル中心をワールド変換
    worldCenter = mul(float4(localCenter, 1.0), world).xyz;

    // 最大スケール軸でバウンディング半径をスケーリング
    float sx = length(float3(world._11, world._12, world._13));
    float sy = length(float3(world._21, world._22, world._23));
    float sz = length(float3(world._31, world._32, world._33));
    worldRadius = localRadius * max(sx, max(sy, sz));
}

// ─── フラスタム判定 ───
bool IsVisible(float3 worldCenter, float worldRadius)
{
    for (int p = 0; p < 6; p++) {
        float dist = dot(frustumPlanes[p].xyz, worldCenter) + frustumPlanes[p].w;
        if (dist < -worldRadius) return false;
    }
    return true;
}

[numthreads(CULL_THREAD_GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    // ★ Y軸 = コマンドインデックス (線形探索不要)
    uint cmdIdx    = groupId.y;
    uint localIdx  = groupId.x * CULL_THREAD_GROUP_SIZE + threadId.x;

    if (cmdIdx >= commandCount) return;

    CullCommandMeta cmd = commands[cmdIdx];

    // このコマンドのインスタンス範囲外なら早期リターン
    if (localIdx >= cmd.instanceCount) return;

    uint inputIdx = cmd.firstInstance + localIdx;

    // ─── ワールド空間バウンズ計算 ───
    float4x4 world = inputInstances[inputIdx].worldMatrix;
    float3 localCenter = float3(cmd.boundsCenterX, cmd.boundsCenterY, cmd.boundsCenterZ);
    float3 worldCenter;
    float  worldRadius;
    TransformBounds(localCenter, cmd.boundsRadius, world, worldCenter, worldRadius);

    // ─── フラスタムテスト ───
    if (!IsVisible(worldCenter, worldRadius)) return;

    // ─── 可視: DrawArgsのinstanceCountをatomic increment ───
    uint drawArgsOffset = cmd.drawArgsIndex * DRAW_ARGS_STRIDE
                        + DRAW_ARGS_INSTANCE_COUNT_OFFSET;
    uint outLocalIdx;
    drawArgsBuffer.InterlockedAdd(drawArgsOffset, 1, outLocalIdx);

    // ─── 出力バッファに書込み ───
    uint writePos = cmd.outputInstanceStart + outLocalIdx;
    outputInstances[writePos] = inputInstances[inputIdx];
}
```

### 6.6 ステート遷移

```
各フレーム:
  ┌─ 前フレーム描画ステートから復帰 ─┐
  │ culledInstance:  VB → UAV           │
  │ culledDrawArgs:  INDIRECT → UAV     │
  └──────────────┬──────────────┘
                 ↓
  ┌─ DrawArgs初期値コピー ──────────┐
  │ culledDrawArgs: UAV → COPY_DEST    │
  │ staging → culledDrawArgs           │
  │ culledDrawArgs: COPY_DEST → UAV    │
  │ (instanceCount=0 で初期化)          │
  └──────────────┬──────────────┘
                 ↓
  ┌─ Dispatch(groupsX, commandCount, 1) ┐
  │ UAV同士の読み書き (UAVバリア不要:    │
  │ 出力バッファは入力と別リソース)       │
  └──────────────┬──────────────┘
                 ↓
  ┌─ 描画用ステート遷移 ──────────┐
  │ culledInstance:  UAV → VB           │
  │ culledDrawArgs:  UAV → INDIRECT     │
  └──────────────┬──────────────┘
                 ↓
        描画パス (Shadow / GBuffer)
```

---

## 7. ExecuteIndirect 仕様

### 7.1 Command Signature
```cpp
D3D12_INDIRECT_ARGUMENT_DESC arg = {};
arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

D3D12_COMMAND_SIGNATURE_DESC desc = {};
desc.ByteStride = DRAW_ARGS_STRIDE;   // 20
desc.NumArgumentDescs = 1;
desc.pArgumentDescs = &arg;
```

### 7.2 呼出し
```cpp
void ExecuteIndexedIndirect(IBuffer* argsBuffer, uint32_t offsetBytes) {
    FlushPSO();
    FlushPendingBarriers();
    m_commandList->ExecuteIndirect(
        m_commandSignature,
        1,                              // コマンド数
        argsBuffer->GetNativeResource(),
        offsetBytes,
        nullptr,                        // カウントバッファ (Phase 3で対応)
        0);
}
```

### 7.3 GPUが読むデータレイアウト
```
argsBuffer + offsetBytes → DRAW_ARGS_STRIDE bytes:
  byte  0.. 3: indexCountPerInstance  (uint32)
  byte  4.. 7: instanceCount         (uint32) ← CPU or Computeが書く
  byte  8..11: startIndexLocation    (uint32)
  byte 12..15: baseVertexLocation    (int32)
  byte 16..19: startInstanceLocation (uint32) ← outputInstanceStart
```

---

## 8. ModelRenderer 描画ロジック

```cpp
void ModelRenderer::RenderPreparedOpaque(RenderContext& rc, ShaderId shaderId)
{
    IBuffer* instanceBuf = rc.activeInstanceBuffer;
    IBuffer* drawArgsBuf = rc.activeDrawArgsBuffer;

    // DX12: ExecuteIndirect、DX11: DrawIndexedInstanced
    const bool useIndirect = (Graphics::Instance().GetAPI() == GraphicsAPI::DX12)
                           && instanceBuf && drawArgsBuf;

    for (const auto& cmd : rc.activeDrawCommands) {
        // ... PSO/シェーダ/メッシュバインド ...

        if (useIndirect) {
            shader->BeginInstanced(rc);
            rc.commandList->SetVertexBuffer(1, instanceBuf, rc.activeInstanceStride, 0);
            shader->Update(rc, *mesh);
            uint32_t offsetBytes = cmd.drawArgsIndex * DRAW_ARGS_STRIDE;
            rc.commandList->ExecuteIndexedIndirect(drawArgsBuf, offsetBytes);
            shader->End(rc);
        } else {
            // DX11 fallback: CPUループ描画
            shader->Begin(rc);
            for (uint32_t i = cmd.firstInstance; i < cmd.firstInstance + cmd.instanceCount; i++) {
                FillAndApplySkeleton(rc, mesh, rc.preparedInstanceData[i]);
                shader->Update(rc, *mesh);
                rc.commandList->DrawIndexed(mesh->indexCount, 0, 0);
            }
            shader->End(rc);
        }
    }

    // スキンドメッシュは常にCPUループ (DX11/DX12共通)
    for (const auto& cmd : rc.activeSkinnedCommands) {
        // ... CPU per-instance描画 ...
    }
}
```

---

## 9. DX11 Fallback 責務の明文化

### 9.1 分岐箇所

| 分岐ポイント | 責任者 | DX11 | DX12 |
|---|---|---|---|
| ComputeCullingPass実行 | EngineKernel | 登録しない | 登録する |
| active*の設定 | BuildIndirectCommandPass | 同じ | 同じ |
| 描画呼出し方法 | ModelRenderer | DrawIndexedInstanced | ExecuteIndirect |
| activeDrawArgsBuffer | BuildIndirectCommandPass | 設定するがRenderer不使用 | 設定→ExecuteIndirect |

### 9.2 DX11で不要なもの
- `activeDrawArgsBuffer` : DX11では参照しない (Rendererがskip)
- ComputeCullingPass : DX11では登録しない
- UAVStorage バッファ : DX11では作成しない

### 9.3 DX11/DX12で共通のもの
- `activeInstanceBuffer` : 両方でVB slot1に使用
- `activeDrawCommands` : 両方でコマンドループに使用
- `activeSkinnedCommands` : 両方でCPU描画に使用
- `IndirectDrawCommand` 構造体 : 同一
- `InstanceData` 構造体 : 同一
- `DrawArgs` 構造体 : DX11では描画に使わないが構築は同一ロジック

---

## 10. 実装チェックリスト

### Phase 1: Indirect Draw基盤 (ComputeCulling無し)
- [ ] `IndirectDrawCommon.h` に共通定数・構造体定義
- [ ] `IndirectDrawCommon.hlsli` にHLSL側定数定義
- [ ] RenderContextに `active*` フィールド追加
- [ ] BuildInstanceBufferPass: `rc.activeInstanceBuffer` 設定
- [ ] BuildIndirectCommandPass: `rc.activeDrawArgsBuffer`, `rc.activeDrawCommands` 設定
- [ ] ModelRenderer: `active*` のみ参照、DX11/DX12分岐を `useIndirect` フラグで
- [ ] ShadowPass: `active*` 経由でインスタンス影描画
- [ ] テスト: DX12でモデル描画+影が安定動作 (10秒以上)

### Phase 2: Compute Culling
- [ ] `CullCommandMeta` 構造体定義 (outputInstanceStart / drawArgsIndex 分離)
- [ ] `CullingParams` 構造体定義 (maxInstancesPerCmd追加)
- [ ] FrustumCullCS.hlsl 実装:
  - [ ] 2Dディスパッチ (`SV_GroupID.y` = コマンドインデックス)
  - [ ] `TransformBounds()` でローカル→ワールド変換
  - [ ] maxScale反映の `worldRadius`
  - [ ] `DRAW_ARGS_STRIDE` / `DRAW_ARGS_INSTANCE_COUNT_OFFSET` 定数使用
- [ ] ComputeCullingPass C++側:
  - [ ] IBuffer経由でステージングバッファ管理 (raw API排除)
  - [ ] Map失敗時ガード (nullチェック後にmemcpy)
  - [ ] Compute後に `rc.active*` を上書き
  - [ ] RestoreDescriptorHeap + GlobalRootSignature::BindAll
  - [ ] `Dispatch(groupsX, commandCount, 1)` (2Dディスパッチ)
- [ ] テスト: カリング有効で描画安定 (10秒以上)

### Phase 3: 最適化
- [ ] カウントバッファ対応 (ExecuteIndirect maxCommandCount > 1)
- [ ] Multi-draw: マテリアル同一コマンド群を1回のExecuteIndirectで実行
- [ ] Hi-Zオクルージョンカリング
- [ ] LOD距離選択
