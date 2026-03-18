// Effect.hlsl

struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 position : POSITION;
    float3 tangent : TANGENT;
    float3 shadow : SHADOW;
    float4 color : COLOR;
};

// ---------------------------------------------------------
// 定数バッファ (b0) : シーン共通 (カメラ、ライト)
// ---------------------------------------------------------
cbuffer CbScene : register(b0)
{
    row_major float4x4 viewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    row_major float4x4 lightViewProjection;
    float4 shadowColor;
    // ... 必要に応じてパディング
};


cbuffer MaterialConstants : register(b1)
{
    // [Block 0]
    float4 baseColor;

    // [Block 1]
    float emissiveIntensity;
    float currentTime;
    float2 mainUvScrollSpeed;

    // [Block 2]
    float distortionStrength;
    float maskEdgeFade;
    float2 distortionUvScrollSpeed;

    // [Block 3]
    float dissolveThreshold;
    float dissolveEdgeWidth;
 
    float maskIntensity;
    float maskContrast;
   
    // [Block 4]
    float3 dissolveEdgeColor;
    float _padding3;

    // ★追加 [Block 5]
    int mainTexIndex;
    int distortionTexIndex;
    int dissolveTexIndex;
    int maskTexIndex;
    
    // ★追加 [Block 6]
    float3 fresnelColor;
    float fresnelPower;
    
    // ★追加 [Block 7]
    float flipbookWidth; // Columns
    float flipbookHeight; // Rows
    float flipbookSpeed; // Speed

    float gradientStrength; // ★追加

    // [Block 8] ★追加
    int gradientTexIndex;
    float wpoStrength;
    float wpoSpeed;
    float wpoFrequency;
    
    // [Block 9] ★追加
    float chromaticAberrationStrength;
    float dissolveGlowIntensity; // ★追加
    float dissolveGlowRange; // ★追加
    
    int uvScrollMode;
    
 

    // [Block 10] ★追加
    float3 dissolveGlowColor;
    int matCapTexIndex; 

    // [Block 11] ★追加
    float matCapStrength;
    float matCapBlend;
    
    
    float clipSoftness; 
    float startEndFadeWidth; 
    
    // [Block 12] ★追加
    float3 matCapColor;

    int normalTexIndex; // ★追加

    // [Block 13] ★追加
    float normalStrength;
    float _padding13_1;
    float _padding13_2;
    float _padding13_3;
    
    // [Block 14]
    int flowTexIndex;
    float flowStrength;
    float flowSpeed;
    float sideFadeWidth;
    
    // [Block 15] Master Controller
    float visibility;
    float clipStart; // 追加
    float clipEnd; // 追加
    float _padding15;
    // ★追加 [Block 16]
    int subTexIndex;
    int subBlendMode;
    float2 subUvScrollSpeed;

    // ★追加 [Block 17]
    float subTexStrength;
    float subUvRotationSpeed; // ★追加
    float usePolarCoords; // ★追加
    
    float toonThreshold; // ★追加

    // ★追加 [Block 18]
    float toonSmoothing;
    float toonSteps; 

    float toonNoiseStrength;
    float toonNoiseSpeed;
    
    float4 toonShadowColor;
    
    int toonNoiseTexIndex;
    float3 _padding20;
    float4 toonSpecular;
    
    int toonRampTexIndex;
    float3 _padding22;
};