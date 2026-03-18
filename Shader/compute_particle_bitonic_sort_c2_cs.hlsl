#include "compute_particle.hlsli"
#include "compute_particle_bitonic_sort.hlsli"

groupshared particle_header shared_data[BitonicSortC2Thread * 2];

[numthreads(BitonicSortC2Thread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int t = dispatch_thread_id.x; // thread index
    int wgBits = 2 * BitonicSortC2Thread - 1; // bit mask to get index in local memory AUX (size is 2*WG)

    for (int inc = increment; inc > 0; inc >>= 1)
    {
        int low = t & (inc - 1); // low order bits (below INC)
        int i = (t << 1) - low; // insert 0 at position INC
        bool reverse = ((direction & i) == 0); // asc/desc order
        particle_header x0, x1;

		// Load
        if (inc == (int) increment)
        {
			// First iteration: load from global memory
            x0 = particle_header_buffer[i];
            x1 = particle_header_buffer[i + inc];
        }
        else
        {
			// Other iterations: load from local memory
            GroupMemoryBarrierWithGroupSync();
            x0 = shared_data[i & wgBits];
            x1 = shared_data[(i + inc) & wgBits];
        }

		// Sort
        {
            particle_header auxa = x0;
            particle_header auxb = x1;
            if (reverse ^ comparer(x0, x1))
            {
                x0 = auxb;
                x1 = auxa;
            }
        }

		// Store
        if (inc == 1)
        {
			// Last iteration: store to global memory
            particle_header_buffer[i] = x0;
            particle_header_buffer[i + inc] = x1;
        }
        else
        {
			// Other iterations: store to local memory
            GroupMemoryBarrierWithGroupSync();
            shared_data[i & wgBits] = x0;
            shared_data[(i + inc) & wgBits] = x1;
        }
    }
}
