// メッシュパーティクル VS（SoA）: AliveList + BillboardHot/Warm/Header + MeshAttribHot を読む。
// 描画: DrawIndexedInstanced(indexCount, aliveCount, 0, 0, 0)
// 入力: 既存 mesh VB との互換性のため BONE レイアウトを維持する。
#include "compute_particle.hlsli"
#include "EffectParticleSoA.hlsli"

StructuredBuffer<uint>              g_AliveList       : register(t0);
StructuredBuffer<BillboardHot>      g_BillboardHot    : register(t1);
StructuredBuffer<BillboardWarm>     g_BillboardWarm   : register(t2);
StructuredBuffer<BillboardHeader>   g_BillboardHeader : register(t3);
StructuredBuffer<MeshAttribHot>     g_MeshAttribHot   : register(t4);

struct VS_IN
{
    float3 pos : POSITION;
    float4 boneWeights : BONE_WEIGHTS;
    uint4 boneIndices : BONE_INDICES;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 color : COLOR0;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float3 normal : TEXCOORD1;
};

VS_OUT main(VS_IN input, uint instanceID : SV_InstanceID)
{
    VS_OUT output = (VS_OUT)0;

    // alive list 経由で Instance から slot へ変換する。
    const uint slot = g_AliveList[instanceID];

    // 安全策: dead slot は縮退三角形へ潰してスキップする。
    const BillboardHeader header = g_BillboardHeader[slot];
    if (!HeaderIsAlive(header.packed)) {
        output.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    const BillboardHot    hot    = g_BillboardHot[slot];
    const BillboardWarm   warm   = g_BillboardWarm[slot];
    const MeshAttribHot   mattr  = g_MeshAttribHot[slot];

    // パーティクルごとの変換手順。
    // 1. 軸別ローカルスケール、2. quaternion 回転、3. ワールド位置への平行移動。
    const float3 localPosScaled = input.pos * mattr.scale;
    const float3 worldPos = QuatRotate(localPosScaled, mattr.rotation) + hot.position;
    const float3 worldNormal = normalize(QuatRotate(input.normal, mattr.rotation));

    // 色: Warm stream の packed color にライフサイクル tint を適用する。
    const float4 lifeColor = UnpackRGBA8(warm.packedColor);

    output.position = mul(float4(worldPos, 1.0f), viewProjection);
    output.texcoord = input.uv;
    output.normal = worldNormal;
    output.color = input.color * lifeColor;
    output.color.a *= global_alpha;
    return output;
}
