// ==================================================================================
// ParticleMeshVS.hlsl
// メッシュパーティクル用 頂点シェーダー
// ==================================================================================

#include "ParticleMesh.hlsli"

// エントリーポイント
VS_OUT main(VS_IN input, uint instanceID : SV_InstanceID)
{
    VS_OUT output = (VS_OUT) 0;

    // 1. インスタンスIDから、この粒子のデータを取得
    particle_data p = g_Particles[instanceID];

    // 2. 形状変形 (スケール -> 回転 -> 移動)
    // input.pos はメッシュのローカル座標
    float3 localPos = input.pos * p.scale.xyz;
    float3 worldPos = RotateVector(localPos, p.rotation) + p.position.xyz;
    
    // 法線・接線の回転
    float3 worldNormal = normalize(RotateVector(input.normal, p.rotation));
    float3 worldTangent = normalize(RotateVector(input.tangent, p.rotation));

    // 3. 座標変換 (ワールド -> スクリーン)
    // viewProjection は Effect.hlsl の CbScene に定義されている
    output.vertex = mul(float4(worldPos, 1.0f), viewProjection);
    
    // ピクセルシェーダーへ渡すワールド座標
    output.position = worldPos;
    
    // 法線・接線
    output.normal = worldNormal;
    output.tangent = worldTangent;
    
    // 影用座標 (EffectVSと同様の計算)
    float4 shadowPos = mul(float4(worldPos, 1.0f), lightViewProjection);
    shadowPos.xyz /= shadowPos.w;
    shadowPos.y = -shadowPos.y;
    shadowPos.xy = shadowPos.xy * 0.5f + 0.5f;
    output.shadow = shadowPos.xyz;

    // 4. 色とUV
    // メッシュの頂点カラー × パーティクルの色
    output.color = input.color * p.color;
    
    // 全体の不透明度 (ConstantBufferから取得、global_alphaはcompute_particle.hlsliにある)
    output.color.a *= global_alpha;

    // UV座標のパススルー
    output.texcoord = input.uv;

    return output;
}