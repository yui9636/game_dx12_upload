//#include "compute_particle.hlsli"

RWStructuredBuffer<particle_header> particle_header_buffer : register(u3);

// ”äŠrŠÖ”
bool comparer(in particle_header x0, in particle_header x1)
{
    return (x0.alive > x1.alive || (x0.alive == x1.alive && x0.depth > x1.depth));
}
