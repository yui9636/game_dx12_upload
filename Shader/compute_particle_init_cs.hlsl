#include "compute_particle.hlsli"

RWStructuredBuffer<particle_data> particle_data_buffer : register(u0); //  パーティクル管理バッファ
AppendStructuredBuffer<int> particle_unused_buffer : register(u1); //  パーティクル番号管理バッファ(末尾への追加専用)
RWStructuredBuffer<particle_header> particle_header_buffer : register(u3); //  パーティクルヘッダー管理バッファ


[numthreads(NumParticleThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int index = dispatch_thread_id.x;

    //  パーティクル情報初期化
    //particle_data_buffer[index].parameter.z = -1.0f;
    particle_header_buffer[index].alive = 0;
    particle_header_buffer[index].particle_index = 0;

    
    //  未使用リスト(AppendStructuredBuffer)の末尾に追加
    particle_unused_buffer.Append(index);
}
