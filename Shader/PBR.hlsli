struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 position : POSITION;
    float3 tangent : TANGENT;
    float   viewDepth : VIEW_DEPTH;
    
    // ★追加: 画面上での位置を計算するための変数
    float4 curClipPos : CUR_CLIP_POS;
    float4 prevClipPos : PREV_CLIP_POS;
};


struct PointLight
{
    float3 position; // 位置
    float range; // 範囲
    float3 color; // 色
    float intensity; // 強度
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
    float shadowTexelSize;
    
    float jitterX;
    float jitterY;
    float renderW;
    float renderH;
    
    float pointLightCount; // 有効なライト数

    float prevJitterX;
    float prevJitterY;
    
    // 配列 (最大8個と仮定)
    PointLight pointLights[8];
};

cbuffer CbMesh : register(b1)
{
    float4 materialColor;
    float metallicFactor;
    float roughnessFactor;
    float emissiveFactor; 
    float occlusionStrength; 
};

cbuffer CbShadowMap : register(b4)
{
    row_major float4x4 lightViewProjections[3]; // 各カスケードの行列
    float4 cascadeSplits; // 分割距離
    float4 shadowColor_CSM; // 影の色
    float4 shadowBias_CSM; // 影バイアス
};

