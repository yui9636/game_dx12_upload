#include "Toon.hlsli"

struct OUT_OUT { float4 vertex : SV_POSITION; };

// Flat outline color. Only backfaces reach here (cull-front rasterizer).
float4 main(OUT_OUT pin) : SV_TARGET
{
    return float4(outlineColor.rgb, 1.0f);
}
