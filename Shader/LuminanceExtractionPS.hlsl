#include "FullScreenQuad.hlsli"
#include"PostEffect.hlsli"


Texture2D colorMap : register(t0);
SamplerState linerSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linerSampler, pin.texcoord) ;

	////RGB > 輝度値に変換
 //   float luminance = RGB2Luminance(color.rgb);

	////闘値との差を算出
 //   float contribution = max(0, luminance - threshold*3);

	////出力する色を補正する
 //   contribution /= luminance;
    color.rgb *= smoothstep(luminanceExtractionLowerEdge, luminanceExtractionHigherEdge, dot(color.rgb, float3(0.299f, 0.587f, 0.114f)));
	
    return color;
}