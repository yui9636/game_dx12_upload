#include "SkyBox.hlsli"

VS_OUT main(uint id : SV_VertexID)
{
    float2 ndc;
    ndc.x = (id == 2 || id == 3) ? 1.0 : -1.0;
    ndc.y = (id == 1 || id == 3) ? -1.0 : 1.0; 

    float4 worldPos = mul(float4(ndc, 1.0, 1.0), inverseViewProjection);
    worldPos /= worldPos.w; 
    float3 eyeDir = normalize(worldPos.xyz);

    VS_OUT o;
    o.svPosition = float4(ndc, 1.0, 1.0); 
    o.rayDir = eyeDir;
    return o;
}