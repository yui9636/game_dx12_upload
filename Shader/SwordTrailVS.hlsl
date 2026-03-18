#include "SwordTrail.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    vout.pos = mul(vin.pos, viewProjection);
    vout.uv = vin.uv;
    vout.vCoord = vin.uv.y; 
    
 
    
    return vout;
}
