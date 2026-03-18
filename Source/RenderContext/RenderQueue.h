#pragma once

#include <vector>
#include <DirectXMath.h>
#include "RenderState.h"
// ECSのコンポーネント(MeshComponent等)は絶対にインクルードしない！
// 純粋な生データ(Model)のポインタだけを知っていれば十分です。
class Model;

// ---------------------------------------------------------
// 1. メッシュ1個分の描画依頼書 (RenderPacket)
// ---------------------------------------------------------
struct RenderPacket {
    Model* model = nullptr;
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 prevWorldMatrix;

    // マテリアルや描画設定
    int shaderId = 1;              // 1 = PBR (ModelRenderer::ShaderId に対応)
    float distanceToCamera = 0.0f; // 半透明のZソート用（奥から手前へ並べるため）
    bool castShadow = true;        // このメッシュは影を落とすか？

    BlendState      blendState = BlendState::Opaque;
    DepthState      depthState = DepthState::TestAndWrite;
    RasterizerState rasterizerState = RasterizerState::SolidCullBack;

    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissive = 0.0f;
};

// ---------------------------------------------------------
// 2. 伝票の束 (RenderQueue 本体)
// ---------------------------------------------------------
// ★ EnvPacket や EditorPacket は廃止され、より適切な場所(Context等)へ移動しました
class RenderQueue {
public:
    // メッシュのリスト（厨房でシェフが捌きやすいように最初から分けておく）
    std::vector<RenderPacket> opaquePackets;      // 不透明なメッシュ
    std::vector<RenderPacket> transparentPackets; // 半透明なメッシュ

    // 毎フレームの最初に「白紙」に戻すための関数
    void Clear() {
        opaquePackets.clear();
        transparentPackets.clear();
    }
};