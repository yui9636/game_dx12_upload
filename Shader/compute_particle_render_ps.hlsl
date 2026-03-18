#include "compute_particle.hlsli"

Texture2D color_map : register(t0);
SamplerState LinearSamp : register(s1);



float4 main(PS_IN pin) : SV_TARGET0
{
    //return color_map.Sample(LinearSamp, pin.texcoord) * pin.color;
    
    float4 color = color_map.Sample(LinearSamp, pin.texcoord) * pin.color;
    
    // ★追加: 全体のフェード値(global_alpha)をアルファに乗算
    color.a *= global_alpha;
    
    return color;
    
    

    
}
