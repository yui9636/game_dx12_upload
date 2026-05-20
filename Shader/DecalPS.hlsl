// Deferred decal pixel shader.
// Reconstructs each covered surface point from the G-buffer, projects it into the
// decal's box-local space, samples the decal texture and outputs a premultiplied-by-
// alpha color that is alpha-blended into GBuffer0 (albedo).

cbuffer CbDecal : register(b1)
{
    float4x4 decalWorldViewProj;
    float4x4 worldToDecal;       // world space -> box-local [-0.5,0.5]
    float4   tintOpacity;        // rgb = tint, a = opacity
    float4   params;             // x = angleFade, y = invWidth, z = invHeight
    float4   decalAxisWS;        // xyz = decal projection axis in world space
};

Texture2D    gWorldPosDepth : register(t0); // GBuffer2: xyz world pos, w clip depth
Texture2D    gNormalRough   : register(t1); // GBuffer1: xyz world normal, w roughness
Texture2D    gDecalTex      : register(t2); // decal albedo
SamplerState gLinearClamp   : register(s0);

float4 main(float4 svPos : SV_POSITION) : SV_TARGET
{
    int3 pix = int3((int)svPos.x, (int)svPos.y, 0);

    float4 gb2 = gWorldPosDepth.Load(pix);
    // Pixels with no geometry (sky) keep a cleared (0,0,0,0) value.
    if (gb2.w <= 0.0f) {
        discard;
    }

    // Project the surface point into the decal box.
    float3 local = mul(float4(gb2.xyz, 1.0f), worldToDecal).xyz;
    float3 a = abs(local);
    if (a.x > 0.5f || a.y > 0.5f || a.z > 0.5f) {
        discard;
    }

    // Box-local XY -> texture UV (V flipped to match texture orientation).
    float2 uv = local.xy + 0.5f;
    uv.y = 1.0f - uv.y;
    float4 texel = gDecalTex.Sample(gLinearClamp, uv);

    // Optional angle fade. Disabled when angleFade <= 0 so a decal simply projects onto
    // every surface inside its box. Raise angleFade to cull surfaces that do not face
    // the projection direction (prevents side-wall smearing).
    float fade = 1.0f;
    if (params.x > 0.0f) {
        float3 n = normalize(gNormalRough.Load(pix).xyz);
        float facing = dot(n, -decalAxisWS.xyz);
        fade = saturate((facing - params.x) / max(1.0f - params.x, 0.001f));
    }

    float alpha = texel.a * tintOpacity.a * fade;
    if (alpha < 0.003f) {
        discard;
    }

    return float4(texel.rgb * tintOpacity.rgb, alpha);
}
