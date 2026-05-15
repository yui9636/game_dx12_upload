Texture2D<float4> splatmapTex  : register(t0);
Texture2D<float4> layer0Albedo : register(t1);
Texture2D<float4> layer1Albedo : register(t2);
Texture2D<float4> layer2Albedo : register(t3);
Texture2D<float4> layer3Albedo : register(t4);
SamplerState      linearWrap   : register(s0);

cbuffer TerrainMaterialCB : register(b1)
{
    float layer0TileScale;
    float layer1TileScale;
    float layer2TileScale;
    float layer3TileScale;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
};

float4 main(PS_INPUT input) : SV_Target
{
    float4 splat = splatmapTex.Sample(linearWrap, input.uv);

    float4 c0 = layer0Albedo.Sample(linearWrap, input.uv * layer0TileScale);
    float4 c1 = layer1Albedo.Sample(linearWrap, input.uv * layer1TileScale);
    float4 c2 = layer2Albedo.Sample(linearWrap, input.uv * layer2TileScale);
    float4 c3 = layer3Albedo.Sample(linearWrap, input.uv * layer3TileScale);

    float4 albedo = c0 * splat.r + c1 * splat.g + c2 * splat.b + c3 * splat.a;

    float3 lightDir = normalize(float3(0.5f, 1.0f, 0.5f));
    float  NdotL    = saturate(dot(normalize(input.normal), lightDir));
    float3 color    = albedo.rgb * (0.3f + 0.7f * NdotL);

    return float4(color, 1.0f);
}
