#include "FullscreenQuad.hlsli"

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
#define LINEAR_BORDER_BLACK 3
#define LINEAR_BORDER_WHITE 4
SamplerState samplerStates[5] : register(s0);
Texture2D textureMap : register(t0);

float4 main(VS_OUT pin) : SV_TARGET
{
    return textureMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
}
