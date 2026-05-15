// Grass GBuffer output (alpha-tested PBR).

cbuffer GrassCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   windDirSpeed;
    float4   windStrengthTime;
    float4   colorBottom;
    float4   colorTop;
    float4   cameraPosition;
};

struct PS_INPUT
{
    float4 position    : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float2 uv          : TEXCOORD1;
    float3 vertexTint  : TEXCOORD2;
    float3 normal      : TEXCOORD3;
    float4 curClipPos  : TEXCOORD4;
    float4 prevClipPos : TEXCOORD5;
};

struct PS_OUTPUT
{
    float4 albedoMetallic  : SV_TARGET0;
    float4 normalRoughness : SV_TARGET1;
    float4 worldPosDepth   : SV_TARGET2;
    float2 velocity        : SV_TARGET3;
};

// Procedural blade alpha shape: thin tapered shape, alpha-tested at tips.
float BladeAlpha(float2 uv)
{
    // uv.x: 0..1 across width, uv.y: 0..1 from top to bottom (root at bottom)
    float centerDist = abs(uv.x - 0.5f) * 2.0f;       // 0 at center, 1 at edge
    // Taper near top: width decreases as we approach top
    float topT = 1.0f - uv.y;                          // 0=bottom, 1=top
    float taper = lerp(1.0f, 0.15f, topT * topT);     // narrower at top
    // a blade exists where centerDist < taper
    return step(centerDist, taper);
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    // Alpha test
    float alpha = BladeAlpha(input.uv);
    if (alpha < 0.5f) discard;

    // Albedo from vertex tint (already includes per-blade variation + gradient)
    float3 albedoSrgb = saturate(input.vertexTint);
    // DeferredLighting expects linear albedo.
    float3 albedoLin  = pow(albedoSrgb, 2.2f);

    float3 N = normalize(input.normal);

    // Velocity
    float2 currentNDC = input.curClipPos.xy  / input.curClipPos.w;
    float2 prevNDC    = input.prevClipPos.xy / input.prevClipPos.w;
    float2 currentUV  = currentNDC * float2(0.5f, -0.5f) + 0.5f;
    float2 prevUV     = prevNDC    * float2(0.5f, -0.5f) + 0.5f;

    output.albedoMetallic  = float4(albedoLin, 0.0f);      // metallic=0 for plants
    output.normalRoughness = float4(N, 0.85f);             // matte grass
    output.worldPosDepth   = float4(input.worldPos, input.position.z);
    output.velocity        = prevUV - currentUV;
    return output;
}
