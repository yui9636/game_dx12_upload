#include "compute_particle.hlsli"

#include "compute_particle_bitonic_sort.hlsli"

[numthreads(BitonicSortB2Thread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint t = dispatch_thread_id.x;
    uint low = t & (increment - 1);
    uint i = (t << 1) - low;
    bool reverse = ((direction & i) == 0);

    particle_header x0 = particle_header_buffer[i];
    particle_header x1 = particle_header_buffer[i + increment];
    particle_header auxa = x0;
    particle_header auxb = x1;
    if (reverse ^ comparer(x0, x1))
    {
        x0 = auxb;
        x1 = auxa;
    }

    particle_header_buffer[i] = x0;
    particle_header_buffer[i + increment] = x1;
}
