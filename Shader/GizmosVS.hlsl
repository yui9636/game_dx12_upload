#include"Gizmos.hlsli"
// main はエディターギズモを単純な色付きプリミティブとして描画する。

VS_OUT main(float4 position:POSITION)
{
    VS_OUT vout;
    vout.position = mul(position, worldViewProjection);
    vout.color =  color;
    
    return vout;
}