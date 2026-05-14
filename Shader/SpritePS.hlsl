#include "sprite.hlsli"

Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);
// main はスプライトの頂点またはピクセル情報を UI / HUD 描画用に処理する。


float4 main(VS_OUT pin) : SV_TARGET
{
	return spriteTexture.Sample(spriteSampler, pin.texcoord) * pin.color;
}
