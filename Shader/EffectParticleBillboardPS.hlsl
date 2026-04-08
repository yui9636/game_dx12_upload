#include "compute_particle.hlsli"

Texture2D color_map : register(t0);
Texture2D<float> scene_depth : register(t1);
SamplerState LinearSamp : register(s1);

float4 main(PS_IN pin) : SV_TARGET0
{
    float4 texel = color_map.Sample(LinearSamp, pin.texcoord);
    const float texelAlpha = texel.a;
    float3 texelRgb = texel.rgb;
    if (texelAlpha > (1.0f / 1024.0f)) {
        texelRgb = saturate(texelRgb / texelAlpha);
    }

    float4 color;
    color.a = texelAlpha * pin.color.a * global_alpha;
    color.rgb = texelRgb * pin.color.rgb;

    if (color.a <= (1.0f / 255.0f)) {
        discard;
    }

    color.rgb *= color.a;

    if (curl_noise_strength > 0.0f) {
        uint depthWidth = 1;
        uint depthHeight = 1;
        scene_depth.GetDimensions(depthWidth, depthHeight);

        const float2 screenUv = saturate(float2(
            pin.position.x / max((float)depthWidth, 1.0f),
            pin.position.y / max((float)depthHeight, 1.0f)));
        const float sceneDepthSample = scene_depth.SampleLevel(LinearSamp, screenUv, 0.0f);
        const float depthDelta = max(sceneDepthSample - pin.position.z, 0.0f);
        const float softFade = saturate(depthDelta * curl_noise_strength);
        color.a *= softFade;
        color.rgb *= softFade;
    }

    return color;
}
