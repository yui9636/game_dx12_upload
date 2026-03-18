#include "compute_particle.hlsli"

RWStructuredBuffer<particle_data> particle_data_buffer : register(u0);
RWByteAddressBuffer indirect_data_buffer : register(u2);
RWStructuredBuffer<particle_header> particle_header_buffer : register(u3);

[numthreads(1, 1, 1)]
void main()
{
    //  パーティクル総数を取得
    uint current_num_particle = indirect_data_buffer.Load(IndirectArgumentsNumCurrentParticle);
    if (current_num_particle == 0)
        return;

    //  簡易的に挿入ソートを行う
    //  クイックソート等の再帰関数を使うものはコンピュートシェーダーの方で制限がかかっているため無理。
    for (int h = current_num_particle / 2; h > 0; h /= 2)
    {
        for (int i = h; i < current_num_particle; i += 1)
        {
            //particle_data k = particle_data_buffer[i];

            //int j;
            //for (j = i; j >= h && (particle_data_buffer[j - h].parameter.z < k.parameter.z); j -= h)
            //{
            //    particle_data_buffer[j] = particle_data_buffer[j - h];
            //}

            //particle_data_buffer[j] = k;
            
            particle_header k = particle_header_buffer[i];

            int j;
            for (j = i; j >= h && (particle_header_buffer[j - h].alive < k.alive); j -= h)
            {
                particle_header_buffer[j] = particle_header_buffer[j - h];
            }

            particle_header_buffer[j] = k;

        }
    }
}
