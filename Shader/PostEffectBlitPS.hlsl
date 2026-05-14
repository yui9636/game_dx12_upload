#include "FullScreenQuad.hlsli"

Texture2D srcTex : register(t0);
SamplerState linearSampler : register(s0);
// main はこのシェーダーステージで必要な入力値を処理し、後段へ渡す出力を生成する。

float4 main(VS_OUT pin) : SV_TARGET
{
    return srcTex.Sample(linearSampler, pin.texcoord);
}
