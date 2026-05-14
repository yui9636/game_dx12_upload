#include"Gizmos.hlsli"
// main はエディターギズモを単純な色付きプリミティブとして描画する。

float4 main(VS_OUT pin):SV_TARGET
{
    return pin.color;
}