// Toon shader shared cbuffers (Neptunia-class).

struct VS_OUT
{
    float4 vertex   : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
    float3 position : POSITION;
    float3 tangent  : TANGENT;
    float3 shadow   : SHADOW;
};

cbuffer CbScene : register(b7)
{
    row_major float4x4 viewProjection;
    row_major float4x4 viewProjectionUnjittered;
    row_major float4x4 prevViewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    row_major float4x4 lightViewProjection;
    float4 shadowColor;
    float  shadowTexelSize;
};

cbuffer CbMesh : register(b1)
{
    float4 materialColor;
};

// Layout (must match ToonShader::CbToon in C++):
//  shadowMid    : rgb = mid shadow color
//  shadowDeep   : rgb = deep shadow color
//  shadowParams : x = midThreshold, y = deepThreshold, z = bandLevels, w = shadingMode (0=Bands,1=ThreeTier,2=Ramp)
//  rimColor     : rgb = rim, a = rimPower
//  rimAux       : x = rimStrength, y = outlineDistanceScale, others unused
//  outlineColor : rgb = outline color, a = outlineWidth
//  specColor    : rgb = spec color, a = specStrength
//  specParams   : x = sharpness (0-1), y = threshold (0-1), z = useFixedLight (0/1), w = useAniso (0/1)
//  fixedLight   : xyz = direction, w = anisoOffset
//  anisoAux     : x = anisoSharpness, others unused
cbuffer CbToon : register(b2)
{
    float4 shadowMid;
    float4 shadowDeep;
    float4 shadowParams;
    float4 rimColor;
    float4 rimAux;
    float4 outlineColor;
    float4 specColor;
    float4 specParams;
    float4 fixedLight;
    float4 anisoAux;
};
