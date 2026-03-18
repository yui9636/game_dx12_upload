# DX12 GPU主導描画改善仕様書

作成日: 2026-03-18  
対象: `C:\Users\yuito\Documents\MyEngine_Workspace\game_dx12_upload`  
前提: DX11 非破壊 / DX12 側のみ段階的改善 / 最終目標は「DX11 と同じ見た目 + DX11 以上の大量描画性能」

---

## 1. 目的

本仕様書の目的は、現在の DX12 描画経路を「DX11 互換の見た目を再現するだけの移植」から脱却させ、以下の 3 条件を同時に満たす描画基盤へ再設計することである。

1. DX11 と同等の見た目を再現できる
2. 複数モデル・大量エンティティ描画で破綻しない
3. Archetype ECS と FrameGraph を活かし、DX12 らしい GPU 主導の高速描画へ拡張できる

---

## 2. 現在の問題認識

### 2.1 現在の DX12 症状

- 1 体のモデルはある程度描画できるが、2 体目以降で破綻しやすい
- 近距離でモデル形状が崩れる
- モデルが合体したように見える
- パス単位では動いていても、draw 間 state / CB / descriptor が汚染されている疑いが強い

### 2.2 根本原因

現在の DX12 経路は、DX11 時代の「1 本の定数バッファを draw ごとに更新して使い回す」前提を色濃く残している。

特に以下が危険である。

- `ModelRenderer` の skeleton 定数バッファを draw ごとに同じ GPU 仮想アドレスへ上書きしている
- `PBRShader` / `GBufferPBRShader` の material / mesh 定数バッファを draw ごとに同じ GPU 仮想アドレスへ上書きしている
- DX12 の `UpdateBuffer()` が単純な upload heap への `memcpy` なので、GPU が前の draw をまだ読んでいない間に後続 draw の値で潰れる

これは DX11 では表面化しにくいが、DX12 では複数 draw / 複数モデルで顕在化する。

---

## 3. 改善方針の大原則

### 3.1 DX11 非破壊

- DX11 の見た目と運用を基準とする
- DX11 コードは原則変更しない
- 変更は DX12 専用実装または API 共通の interface 追加に限定する

### 3.2 CPU 主導の draw ループから、GPU 主導の描画準備へ寄せる

本改善では、最終的に以下の方針へ移行する。

- CPU は「どの archetype / mesh / material が何個あるか」を集約する
- draw ごとの細粒度状態更新は減らし、GPU が読みやすい連続バッファへ詰める
- 個別 draw call を乱発するのではなく、instancing / indirect draw に移行する

### 3.3 FrameGraph を「パス管理」だけでなく「バッファ寿命管理」に使う

現在の FrameGraph は主に render target / intermediate texture 管理に使われている。  
改善後は以下も管理対象に含める。

- per-frame draw buffer
- instance buffer
- material parameter buffer
- visible instance list
- indirect command buffer

---

## 4. 目標アーキテクチャ

### 4.0 Model / ModelResource / ModelRenderer の責務分離

DX12 対応にあたっては、`Model` が現在抱えている

- asset 読み込み
- CPU 側 mesh / material / animation 保持
- GPU バッファ所有
- draw-ready な描画情報保持

の責務を分離する。

これは最適化のための後付けではなく、DX12 の正しい描画設計の前提とする。

#### `Model`

`Model` は asset / scene 論理表現として扱う。

保持する責務:

- ノード階層
- mesh 定義
- material 定義
- animation / bone 情報
- シリアライズ / キャッシュ対象データ
- source asset 由来の CPU データ

保持しない責務:

- API 依存の GPU バッファ所有
- draw ごとの一時 upload 情報
- pass 固有の binding 状態

#### `ModelResource`

`ModelResource` は描画専用 resource 層とする。

保持する責務:

- vertex buffer
- index buffer
- mesh ごとの draw range
- material index / node 参照など draw-ready な mesh metadata
- DX11 / DX12 各 API における GPU resource 所有

役割:

- `Model` から生成される描画用 resource
- 同じ `Model` を複数 entity が使っても共有される
- instancing / indirect draw の基盤になる

#### `ModelRenderer`

`ModelRenderer` は `Model` と `ModelResource` を使って描画を組み立てる層とする。

保持する責務:

- per-frame upload
- batch 化
- instancing
- shader / pipeline 選択
- pass ごとの draw 発行

保持しない責務:

- asset 読み込み
- source ファイルの解決
- モデル自体の永続キャッシュ

#### 導入意図

この分離により、以下が可能になる。

- DX12 の不具合原因を asset 層から切り離せる
- GPU 資源共有を自然に行える
- `ModelRenderer` を CPU draw loop から GPU driven へ進化させやすい
- DX11 は既存 `Model` を維持しつつ、DX12 だけ `ModelResource` の責務を厚くできる

### 4.1 Archetype ECS の役割

Archetype ECS は以下に非常に相性が良い。

- 同じコンポーネント構成の entity を連続メモリで持てる
- `Transform + Mesh + Material + Visibility` を持つ archetype を高速走査できる
- 大量 entity を CPU 側でソート/集約しやすい

改善後の描画抽出は、entity 単位ではなく archetype 単位で進める。

#### 抽出単位

- Archetype ごとに描画対象列を取得
- `MeshComponent.model`
- `TransformComponent.worldMatrix`
- `MaterialComponent.materialAsset`
- 可視/非可視フラグ

これを 1 体ずつ `RenderQueue` に push するのではなく、**batch key 単位で集約**する。

### 4.2 Batch Key

描画の基本単位を以下で表す。

```cpp
struct DrawBatchKey
{
    Model* model;
    uint32_t meshIndex;
    ShaderId shaderId;
    MaterialAsset* material;
    BlendState blend;
    DepthState depth;
    RasterizerState raster;
};
```

同じ key を持つ entity 群は、instancing 候補とする。

### 4.3 Draw Packet から Instance Batch へ

現状:

- `RenderQueue.opaquePackets` に entity ごとの packet を積む

改善後:

- `RenderQueue.instanceBatches` を持つ
- 1 batch の中に複数 instance を格納する

```cpp
struct InstanceData
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 prevWorld;
    DirectX::XMFLOAT4 baseColor;
    float metallic;
    float roughness;
    float emissive;
};

struct InstanceBatch
{
    DrawBatchKey key;
    std::vector<InstanceData> instances;
};
```

---

## 5. DX12 側の設計変更

### 5.1 絶対に必要な土台: Dynamic Upload Ring

現在の最大の欠陥は「同じ定数バッファを draw ごとに上書きしている」ことなので、最優先で以下を導入する。

#### 新規導入

- `DX12DynamicUploadRing`
- `Allocate(size, alignment)` でフレーム内の連続 upload 領域を返す
- 戻り値は
  - CPU pointer
  - GPU virtual address
  - size

```cpp
struct DynamicAllocation
{
    void* cpuPtr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuVA;
    uint32_t size;
};
```

#### 用途

- skeleton CB
- mesh/material CB
- scene pass の一時 CB
- 将来的には instance buffer も同じ allocator から切れるようにする

#### ルール

- DX12 では定数バッファを「永続 1 本 + 上書き」しない
- draw / batch ごとに別領域を切る
- bind 時はその `gpuVA` を直接 root CBV に渡す

### 5.2 Root Signature の再設計

将来の instancing / GPU driven を見据え、root parameter は「頻繁に変わるもの」と「ほぼ固定のもの」に分離する。

#### 現状の問題

- CBV スロットが draw 単位 / pass 単位 / global で混在
- SRV table も場当たり的に高スロット依存がある

#### 改善案

- `b0`: Scene constants
- `b1`: Pass constants
- `b2`: Material constants
- `b3`: Instance constants or batch constants
- `t0` SRV table: material textures
- `t1` SRV table: pass textures (GBuffer, AO, SSR, Fog, IBL)
- static samplers は DX11 契約を維持しつつ DX12 に固定

これにより

- mesh/material 更新
- pass texture 更新
- global scene 更新

を明確に分離できる。

### 5.3 Descriptor Heap の責務分離

現在は

- shared SRV block
- pass 専用 heap
- sky 用 heap
- material 用 heap

が混在している。

改善後は以下に整理する。

1. Global Texture Heap
   - 永続テクスチャ用
   - model material texture / IBL / LUT / sky など

2. Frame Transient Heap
   - FrameGraph 中間テクスチャ用
   - GBuffer / AO / SSR / Fog など

3. UI Heap
   - ImGui 用

pass が個別 heap を勝手に作る方式は、最終的に廃止対象とする。

---

## 6. Instancing 方針

### 6.1 最初の instancing 対象

以下の条件を満たすものから instancing を導入する。

- 同じ model / mesh
- 同じ material
- 同じ pipeline state
- skeletal animation を持たない

つまり最初は static mesh 群から始める。

### 6.2 skinning ありモデル

スキニングありモデルは、最初は instancing 対象外とする。  
ただし、定数バッファの上書き問題は skinning ありでも先に直す必要がある。

第 2 段階で以下を検討する。

- bone palette texture / structured buffer 化
- compute skinning
- skinned instance buffer

### 6.3 HLSL 側の instance 入力

将来的には vertex shader を以下へ移行する。

- vertex buffer: per-vertex
- instance buffer: per-instance

```cpp
POSITION/NORMAL/TANGENT/TEXCOORD  // per-vertex
INSTANCE_WORLD0-3                // per-instance
INSTANCE_PREV_WORLD0-3           // per-instance
INSTANCE_MATERIAL                // per-instance
```

これにより、同一 mesh を 1 draw instanced で大量描画できる。

---

## 7. FrameGraph との統合

### 7.1 新しいパス責務

FrameGraph に以下の「準備パス」を追加する。

#### ExtractVisibleInstancesPass

- Archetype ECS から描画対象 entity を抽出
- batch key ごとに集約
- visible instance list を作成

#### BuildInstanceBufferPass

- instance data を upload ring へ書く
- instance SRV / VB を生成

#### BuildIndirectCommandPass

- 将来的に indirect draw command を GPU/CPU で構築

### 7.2 現行パスの役割変更

- `GBufferPass`
  - 1 entity ずつ draw するパスから
  - `instanceBatches` を消費するパスへ変更

- `ShadowPass`
  - 同じ batch 体系を使う

- `ForwardTransparentPass`
  - 透明は当面 entity 単位で維持
  - 後から sort & batch を検討

---

## 8. 実装フェーズ

### Phase A: 破綻防止

目的:
- 複数モデル破綻を止める

作業:
- `ModelResource` 層を導入し、`Model` から GPU resource 所有を分離する
- `DX12DynamicUploadRing` 導入
- skeleton / material CB を draw ごとに別領域へ切る
- `ModelRenderer`, `PBRShader`, `GBufferPBRShader` を DX12 だけ dynamic allocation 対応
- `ModelRenderer` は `Model` ではなく `ModelResource` の draw-ready 情報を使う

完了条件:
- 2 体以上のモデルを出しても合体・崩壊しない
- `Model` / `ModelResource` / `ModelRenderer` の責務が分離されている

### Phase B: Batch 化

目的:
- Archetype ECS の連続性を活かして CPU draw overhead を削減

作業:
- `RenderQueue` を `opaquePackets` から `instanceBatches` へ拡張
- `MeshExtractSystem` を batch key 集約型へ変更
- `GBufferPass` で batch ごとに draw

完了条件:
- 同一 mesh/material の複数 entity が 1 batch にまとまる

### Phase C: Instancing

目的:
- draw call 数を削減し、DX11 より高速化する

作業:
- per-instance buffer 導入
- instanced input layout 導入
- static mesh の instanced draw 化

完了条件:
- 同一 mesh/material 群が instancing される

### Phase D: GPU Driven 準備

目的:
- indirect draw / GPU culling へ進める基盤を作る

作業:
- visible instance buffer を structured buffer 化
- indirect command buffer の設計
- 将来の compute culling 入口を追加

完了条件:
- CPU 主導の draw packet ループから脱却できる設計になる

---

## 9. DX11 非破壊ルール

以下は明示的に守る。

- DX11 の `ModelRenderer` / `Shader` の public contract は変えない
- DX11 では従来の定数バッファ更新方式を維持してよい
- DX12 用コードは `if (GraphicsAPI::DX12)` または DX12 クラス側へ閉じ込める
- shared HLSL の変更は最小にし、必要なら DX12 用 path を追加する

---

## 10. 期待効果

### 10.1 正しさ

- 2 体以上でのモデル破綻解消
- draw 間 state 汚染の解消
- pass 間での定数バッファ破壊の解消

### 10.2 性能

- draw call 数の削減
- Archetype ECS の連続走査をそのまま描画準備へ使える
- FrameGraph のリソース寿命管理と相性が良い
- 将来的に GPU culling / ExecuteIndirect へ繋げられる

### 10.3 保守性

- DX11 の正を壊さない
- DX12 の責務が
  - resource lifetime
  - per-draw upload
  - descriptor management
  - pass orchestration
 へ整理される

---

## 11. 今すぐ着手すべき最小実装

最優先は以下だけでよい。

1. `DX12DynamicUploadRing` を作る
2. `ModelRenderer` の skeleton CB を draw ごとに別 GPU VA にする
3. `PBRShader` / `GBufferPBRShader` の mesh/material CB を draw ごとに別 GPU VA にする
4. `ModelResource` を導入して `Model` から GPU resource を分離する

ここが終わるだけで、複数モデル破綻の本丸を潰せる可能性が高い。

その後に

5. `RenderQueue` の batch 化
6. static mesh instancing
7. GPU driven 化

へ進む。

---

## 12. 結論

現在の DX12 問題は「シェーダー計算が違う」よりも、

- DX11 時代の 1 draw 前提データ更新
- DX12 の非同期 GPU 実行
- per-draw resource 管理不足

の衝突によって起きている。

したがって、最適な改善方針は

- DX11 非破壊
- DX12 だけ dynamic upload / batch / instancing / GPU driven へ段階移行

である。

この方針なら、単に DX11 と同じ見た目へ寄せるだけでなく、最終的に DX11 より大量描画に強い基盤へ進化できる。
