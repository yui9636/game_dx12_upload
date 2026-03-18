// ==========================================
// GTAOPS.hlsl (The Ultimate Mathematical Fix)
// ==========================================
#include "PBR.hlsli"

// GTAOPass 側で GBuffer の Target 1(Normal) を t0 に、Target 2(WorldPos) を t1 にセットします。
Texture2D NormalMap : register(t0);
Texture2D WorldPosMap : register(t1);

SamplerState PointSamp : register(s2);

struct VS_OUT_QUAD
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float IGN(float2 pixelPos)
{
    return frac(52.9829189f * frac(dot(pixelPos, float2(0.06711056f, 0.00583715f))));
}

float4 main(VS_OUT_QUAD pin) : SV_TARGET
{
    float3 P = WorldPosMap.Sample(PointSamp, pin.uv).xyz;
    float3 N = NormalMap.Sample(PointSamp, pin.uv).xyz;
    
    if (length(N) < 0.1f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    N = normalize(N);

    // ==========================================
    // 1. カメラからの距離と視線ベクトルを計算
    // ==========================================
    float3 V_cam = P - cameraPosition.xyz;
    float distToCam = length(V_cam);
    if (distToCam < 0.1f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    V_cam /= distToCam;

    const int SLICES = 4;
    const int STEPS = 4;
    const float RADIUS = 1.0f;
    float ao = 0.0f;
    
    // ==========================================
    // 2. テンポラルノイズの完全化
    // ==========================================
    // FSR2のジッターを乱数シードとして加算し、毎フレーム異なるノイズをTAAに平均化させます
    float temporalSeed = jitterX * 1337.0f + jitterY * 7331.0f;
    float noise = IGN(pin.pos.xy + temporalSeed);
    
    // ==========================================
    // 3. 絶対的で安全なUV投影半径の算出（震えの完全抹殺）
    // ==========================================
    // 視線に対して垂直な「右」ベクトルを生成し、ワールド空間でRADIUS分ずらした点を作る
    float3 right = cross(V_cam, float3(0.0f, 1.0f, 0.0f));
    if (length(right) < 0.001f)
        right = cross(V_cam, float3(1.0f, 0.0f, 0.0f));
    right = normalize(right);
    
    // 中心(P)と、ずらした点(P + right * RADIUS)をそれぞれクリップ空間へ変換
    float4 clipCenter = mul(float4(P, 1.0f), viewProjectionUnjittered);
    float4 clipOffset = mul(float4(P + right * RADIUS, 1.0f), viewProjectionUnjittered);
    
    // クリップ空間(-1 ? 1)の座標差分から、UV空間(0 ? 1)の絶対的な半径を計算する
    float2 ndcCenter = clipCenter.xy / clipCenter.w;
    float2 ndcOffset = clipOffset.xy / clipOffset.w;
    float radiusUV = length(ndcOffset - ndcCenter) * 0.5f;
    
    float aspectRatio = renderW / renderH;

    for (int i = 0; i < SLICES; ++i)
    {
        float phi = (i + noise) * (3.14159265f / SLICES);
        float2 dir = float2(cos(phi), sin(phi));
        
        // アスペクト比補正（真円を担保）
        dir.y *= aspectRatio;
        
        float maxAngle = -1.0f;
        
        for (int j = 1; j <= STEPS; ++j)
        {
            float2 offset = dir * (j / (float) STEPS) * radiusUV;
            float2 sampleUV = pin.uv + offset;
            
            if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
                continue;

            float3 samplePos = WorldPosMap.Sample(PointSamp, sampleUV).xyz;
            float3 V = samplePos - P;
            float dist = length(V);

            if (dist < 0.05f || dist > RADIUS)
                continue;

            V /= dist;
            float angle = dot(N, V);
            maxAngle = max(maxAngle, angle);
        }
        ao += max(0.0f, maxAngle);
    }
    
    ao = 1.0f - (ao / SLICES);
    return float4(ao, ao, ao, 1.0f);
}