//=============================================================================
// HologramVS.hlsl
//=============================================================================
#include "Hologram.hlsl"
#include "Skinning.hlsli" // 既存のスキニング関数を利用

// 簡易乱数生成 (グリッチ用)
float hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    float3 normal : NORMAL,
    float3 tangent : TANGENT
)
{
    VS_OUT vout;

    // 1. スキニング適用 (現在のポーズに変形)
    // SkinningPosition / SkinningVector は Skinning.hlsli 内の関数
    float4 skinnedPos = SkinningPosition(position, boneWeights, boneIndices);
    float3 skinnedNormal = SkinningVector(normal, boneWeights, boneIndices);

    // 2. グリッチノイズ (頂点シェーダーで形状を乱す)
    // 強度が 0 より大きい時だけ計算
    if (glitchIntensity > 0.001)
    {
        // 時間と高さ(Y)を種にしてランダム値を生成
        float noise = hash(float2(time * 20.0, skinnedPos.y));
        
        // 閾値を超えた部分だけ横に飛ばす (ブロックノイズ風)
        if (noise > 0.96)
        {
            float shift = (noise - 0.5) * glitchIntensity * 2.0;
            skinnedPos.x += shift;
            skinnedPos.z += shift;
        }
    }

    // 3. 座標変換
    vout.position = mul(skinnedPos, viewProjection); // クリップ座標へ
    vout.worldPos = skinnedPos.xyz; // ワールド座標はピクセルシェーダーへ
    vout.normal = normalize(skinnedNormal);
    vout.texcoord = texcoord;

    return vout;
}