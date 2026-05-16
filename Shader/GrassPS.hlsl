// Grass GBuffer output (alpha-tested against the model's albedo alpha).

cbuffer GrassCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   windDirSpeed;
    float4   windStrengthTime;       // x=strength, y=time, z=alphaCutoff
    float4   colorBottom;
    float4   colorTop;
    float4   cameraPosition;
};

Texture2D    gAlbedo : register(t0);
SamplerState gLinearWrap : register(s0);

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

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    float4 tex = gAlbedo.Sample(gLinearWrap, input.uv);
    float alphaCutoff = max(windStrengthTime.z, 0.05f);
    // Detect missing/black texture so the blade stays visible even without art.
    float texEnergy = tex.r + tex.g + tex.b + tex.a;
    bool hasTex = texEnergy > 0.001f;
    if (hasTex && tex.a < alphaCutoff) discard;

    // Combine sampled albedo with per-vertex tint (gradient + per-instance variance).
    float3 baseSrgb   = hasTex ? tex.rgb : float3(1.0f, 1.0f, 1.0f);
    float3 albedoSrgb = saturate(baseSrgb * input.vertexTint);
    float3 albedoLin  = pow(albedoSrgb, 2.2f);

    float3 N = normalize(input.normal);

    // Velocity
    float2 currentNDC = input.curClipPos.xy  / input.curClipPos.w;
    float2 prevNDC    = input.prevClipPos.xy / input.prevClipPos.w;
    float2 currentUV  = currentNDC * float2(0.5f, -0.5f) + 0.5f;
    float2 prevUV     = prevNDC    * float2(0.5f, -0.5f) + 0.5f;

    output.albedoMetallic  = float4(albedoLin, 0.0f);
    output.normalRoughness = float4(N, 0.85f);
    output.worldPosDepth   = float4(input.worldPos, input.position.z);
    output.velocity        = prevUV - currentUV;
    return output;
}
