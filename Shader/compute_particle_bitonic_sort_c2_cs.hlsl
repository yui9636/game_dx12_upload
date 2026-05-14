#include "compute_particle.hlsli"
#include "compute_particle_bitonic_sort.hlsli"

groupshared particle_header shared_data[BitonicSortC2Thread * 2];

[numthreads(BitonicSortC2Thread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int t = dispatch_thread_id.x; // スレッド index。
    int wgBits = 2 * BitonicSortC2Thread - 1; // ローカルメモリ AUX 内の index を得るための bit mask。サイズは 2*WG。

    for (int inc = increment; inc > 0; inc >>= 1)
    {
        int low = t & (inc - 1); // INC より下位の bit。
        int i = (t << 1) - low; // INC の位置へ 0 bit を挿入した index。
        bool reverse = ((direction & i) == 0); // 昇順 / 降順の向き。
        particle_header x0, x1;

		// 入力データを読み込む。
        if (inc == (int) increment)
        {
			// 初回反復では global memory から読み込む。
            x0 = particle_header_buffer[i];
            x1 = particle_header_buffer[i + inc];
        }
        else
        {
			// 2 回目以降は local memory から読み込む。
            GroupMemoryBarrierWithGroupSync();
            x0 = shared_data[i & wgBits];
            x1 = shared_data[(i + inc) & wgBits];
        }

		// 比較交換で並び替える。
        {
            particle_header auxa = x0;
            particle_header auxb = x1;
            if (reverse ^ comparer(x0, x1))
            {
                x0 = auxb;
                x1 = auxa;
            }
        }

		// 並び替え結果を書き戻す。
        if (inc == 1)
        {
			// 最終反復では global memory へ書き戻す。
            particle_header_buffer[i] = x0;
            particle_header_buffer[i + inc] = x1;
        }
        else
        {
			// 途中反復では local memory へ書き戻す。
            GroupMemoryBarrierWithGroupSync();
            shared_data[i & wgBits] = x0;
            shared_data[(i + inc) & wgBits] = x1;
        }
    }
}
