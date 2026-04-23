// ============================================================================
// Mesh Particle VS (SoA): reads AliveList + BillboardHot/Warm/Header + MeshAttribHot
// Draw: DrawIndexedInstanced(indexCount, aliveCount, 0, 0, 0)
// Input: BONE layout retained for compatibility with existing mesh VB
// ============================================================================

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

    // Instance -> slot via alive list
    const uint slot = g_AliveList[instanceID];

    // Safety: skip dead slots by collapsing to degenerate triangle
    const BillboardHeader header = g_BillboardHeader[slot];
    if (!HeaderIsAlive(header.packed)) {
        output.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    const BillboardHot    hot    = g_BillboardHot[slot];
    const BillboardWarm   warm   = g_BillboardWarm[slot];
    const MeshAttribHot   mattr  = g_MeshAttribHot[slot];

    // Per-particle transform:
    // 1) local scale (per-axis) -> 2) rotate by quaternion -> 3) translate to world position
    const float3 localPosScaled = input.pos * mattr.scale;
    const float3 worldPos = QuatRotate(localPosScaled, mattr.rotation) + hot.position;
    const float3 worldNormal = normalize(QuatRotate(input.normal, mattr.rotation));

    // Color: lifecycle-tinted packed color from Warm stream
    const float4 lifeColor = UnpackRGBA8(warm.packedColor);

    output.position = mul(float4(worldPos, 1.0f), viewProjection);
    output.texcoord = input.uv;
    output.normal = worldNormal;
    output.color = input.color * lifeColor;
    output.color.a *= global_alpha;
    return output;
}
