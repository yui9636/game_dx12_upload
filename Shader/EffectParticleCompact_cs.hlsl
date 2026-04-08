#include "EffectParticleRuntimeCommon.hlsli"

StructuredBuffer<particle_header> particle_alive_list_buffer : register(t0);
RWStructuredBuffer<particle_data> particle_data_buffer : register(u0);
RWStructuredBuffer<particle_header> particle_header_buffer : register(u1);
RWStructuredBuffer<uint> particle_dead_list_buffer : register(u2);
RWByteAddressBuffer particle_counter_buffer : register(u3);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint slot = dispatchThreadId.x;
    const uint capacity = (uint)gCameraDirectionCapacity.w;
    if (slot >= capacity) {
        return;
    }

    particle_data particle = particle_data_buffer[slot];
    const bool alive = (particle.parameter.w > 0.0f) && (particle.parameter.y > 0.0f);

    particle_header header;
    header.alive = alive ? 1u : 0u;
    header.particle_index = slot;
    header.dummy = 0u;

    if (alive) {
        const float3 cameraToParticle = particle.position.xyz - gCameraPositionSortSign.xyz;
        header.depth = dot(cameraToParticle, gCameraDirectionCapacity.xyz) * gCameraPositionSortSign.w;

        uint activeIndex = 0u;
        particle_counter_buffer.InterlockedAdd(EffectParticleCounterActiveCountOffset, 1u, activeIndex);
        particle_header_buffer[activeIndex] = header;
        return;
    }

    particle.parameter.w = 0.0f;
    particle.scale = float4(0.0f, 0.0f, 0.0f, 1.0f);
    particle.color.a = 0.0f;
    particle_data_buffer[slot] = particle;

    uint deadIndex = 0u;
    particle_counter_buffer.InterlockedAdd(EffectParticleCounterDeadCountOffset, 1u, deadIndex);
    uint killedCount = 0u;
    particle_counter_buffer.InterlockedAdd(EffectParticleCounterKillCountOffset, 1u, killedCount);
    particle_dead_list_buffer[deadIndex] = slot;
    header.depth = EffectParticleDeadDepth;
    particle_header_buffer[capacity - 1u - deadIndex] = header;
}
