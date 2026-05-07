#include "FullScreenQuad.hlsli"

Texture2D srcTex : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    return srcTex.Sample(linearSampler, pin.texcoord);
}
