#include "Toon.hlsli"
#include "Skinning.hlsli"

// Inverted-hull outline: extrude vertex along normal, render backfaces in flat color.
// Distance scaling: when rimAux.y > 0, widen outline at near distance, narrow at far.
struct OUT_OUT { float4 vertex : SV_POSITION; };

OUT_OUT main(
    float4 position    : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4  boneIndices : BONE_INDICES,
    float2 texcoord    : TEXCOORD,
    float3 normal      : NORMAL,
    float3 tangent     : TANGENT)
{
    OUT_OUT vout;
    position = SkinningPosition(position, boneWeights, boneIndices);
    float3 N = normalize(SkinningVector(normal, boneWeights, boneIndices));

    float width = outlineColor.a;
    float distScale = 1.0f;
    if (rimAux.y > 0.001f) {
        float3 toCam = cameraPosition.xyz - position.xyz;
        float dist = length(toCam);
        // At dist=2, scale=1.0; at dist=10, scale=0.3 (clamped)
        float falloff = saturate(1.0f - (dist - 2.0f) * 0.08f);
        distScale = lerp(1.0f, falloff, saturate(rimAux.y));
        distScale = max(0.25f, distScale);
    }

    position.xyz += N * width * distScale;

    vout.vertex = mul(position, viewProjection);
    return vout;
}
