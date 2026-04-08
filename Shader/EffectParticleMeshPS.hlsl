#include "compute_particle.hlsli"

Texture2D color_map : register(t2);
SamplerState LinearSamp : register(s1);

struct PARTICLE_MESH_PS_IN
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float3 normal : TEXCOORD1;
};

float4 main(PARTICLE_MESH_PS_IN pin) : SV_TARGET0
{
    float3 normal = normalize(pin.normal);
    float3 lightDir = normalize(-lightDirection.xyz);
    float lighting = saturate(dot(normal, lightDir)) * 0.7f + 0.3f;
    float4 texColor = color_map.Sample(LinearSamp, pin.texcoord);
    float4 outColor = texColor * pin.color;
    outColor.rgb *= lighting;
    outColor.a *= global_alpha;
    return outColor;
}
