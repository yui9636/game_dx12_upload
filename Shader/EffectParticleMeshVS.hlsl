#include "compute_particle.hlsli"

StructuredBuffer<particle_data> particle_data_buffer : register(t0);
StructuredBuffer<particle_header> particle_header_buffer : register(t1);

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

float3 RotateVector(float3 v, float4 q)
{
    float3 t = 2.0f * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

VS_OUT main(VS_IN input, uint instanceID : SV_InstanceID)
{
    VS_OUT output = (VS_OUT)0;

    particle_header header = particle_header_buffer[instanceID];
    particle_data particle = particle_data_buffer[header.particle_index];
    float alive = header.alive != 0 ? 1.0f : 0.0f;

    float3 localPos = input.pos * particle.scale.xyz * alive;
    float3 worldPos = RotateVector(localPos, particle.rotation) + particle.position.xyz;
    float3 worldNormal = normalize(RotateVector(input.normal, particle.rotation));

    output.position = mul(float4(worldPos, 1.0f), viewProjection);
    output.texcoord = input.uv;
    output.normal = worldNormal;
    output.color = input.color * particle.color;
    output.color.a *= global_alpha;
    return output;
}
