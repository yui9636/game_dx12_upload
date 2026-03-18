#include "compute_particle.hlsli"

RWByteAddressBuffer indirect_data_buffer : register(u2);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint index = dispatch_thread_id.x;

    //  1F前フレームの総パーティクル数＋現在フレームの生成パーティクル数＝"仮の"現在フレームの総パーティクル数
    uint previous_num_particle = indirect_data_buffer.Load(IndirectArgumentsNumCurrentParticle);
    uint current_num_particle = previous_num_particle + total_emit_count;
    
    //  現在フレームの総パーティクル数はシステムの総パーティクル数で制限
    current_num_particle = min(system_num_particles, current_num_particle);

    //  総数を記録
    indirect_data_buffer.Store(IndirectArgumentsNumCurrentParticle, current_num_particle);
    indirect_data_buffer.Store(IndirectArgumentsNumPreviousParticle, previous_num_particle);

    indirect_data_buffer.Store(IndirectArgumentsDrawIndirect + 4, current_num_particle);
    
    //  死亡カウンターを初期化
    indirect_data_buffer.Store(IndirectArgumentsNumDeadParticle, 0);
    
    //  エミッター用の dispatch indirect に起動数を設定する
    uint3 emit_dispatch;
    emit_dispatch.x = current_num_particle - previous_num_particle;
    emit_dispatch.y = 1;
    emit_dispatch.z = 1;
    indirect_data_buffer.Store3(IndirectArgumentsEmitParticleDispatchIndirect, emit_dispatch);
    
    //  エミッターの生成番号を設定
    //  ソートするので1F前のパーティクル数がそのままエミット番号になる
    indirect_data_buffer.Store(IndirectArgumentsNumEmitParticleIndex, previous_num_particle);

    //  更新用の dispatch indirect に起動数を設定する
    uint3 update_dispatch;
    update_dispatch.x = ((current_num_particle + (NumParticleThread - 1)) / NumParticleThread);
    update_dispatch.y = 1;
    update_dispatch.z = 1;
    indirect_data_buffer.Store3(IndirectArgumentsUpdateParticleDispatchIndirect, update_dispatch);

}
