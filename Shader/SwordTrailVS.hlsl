#include "SwordTrail.hlsli"
// main は軌跡描画用の頂点情報を変換し、後段の描画処理へ渡す。

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    vout.pos = mul(vin.pos, viewProjection);
    vout.uv = vin.uv;
    vout.vCoord = vin.uv.y; 
    
 
    
    return vout;
}
