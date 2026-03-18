#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u2);

[numthreads(1, 1, 1)]
void main()
{
    //  死亡カウンターを取得&初期化
    uint destroy_counter = indirect_data_buffer.Load(IndirectArgumentsNumDeadParticle);
    indirect_data_buffer.Store(IndirectArgumentsNumDeadParticle, 0);
    
    //  現在フレームでのパーティクル総数を再計算
    uint current_num_particle = indirect_data_buffer.Load(IndirectArgumentsNumCurrentParticle);
    indirect_data_buffer.Store(IndirectArgumentsNumCurrentParticle, current_num_particle - destroy_counter);
    
     //  描画コール数をここで決める
    indirect_data_buffer.Store(IndirectArgumentsDrawIndirect, current_num_particle - destroy_counter);
}
