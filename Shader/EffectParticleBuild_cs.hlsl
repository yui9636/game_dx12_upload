#define _COMPUTE_PARTICLE_DISABLE_CBSCENE_
#include "compute_particle.hlsli"

cbuffer EffectParticleBuildParams : register(b0)
{
    float4 gOriginTime;
    float4 gTint;
    float4 gTintEnd;
    float4 gTiming;
    float4 gCameraPositionSortSign;
    float4 gCameraDirectionCapacity;
    float4 gAccelerationDrag;
    float4 gShapeParams;
    float4 gShapeTypeSpin;
    float gStartSize;
    float gEndSize;
    uint gMaxParticles;
    uint gSeed;
}

RWStructuredBuffer<particle_data> particle_data_buffer : register(u0);
RWStructuredBuffer<particle_header> particle_header_buffer : register(u1);

float Hash01(uint value)
{
    value ^= value >> 17;
    value *= 0xED5AD4BBu;
    value ^= value >> 11;
    value *= 0xAC4C1B51u;
    value ^= value >> 15;
    value *= 0x31848BABu;
    value ^= value >> 14;
    return (value & 0x00FFFFFFu) / 16777215.0f;
}

float3 RandomDirection(uint index, uint seed)
{
    const float rand0 = Hash01(index * 9781u + seed * 6271u + 17u);
    const float rand1 = Hash01(index * 6271u + seed * 9781u + 53u);
    const float angle = rand0 * 6.2831853f;
    const float z = rand1 * 2.0f - 1.0f;
    const float r = sqrt(saturate(1.0f - z * z));
    return float3(cos(angle) * r, z, sin(angle) * r);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    const uint capacity = (uint) gCameraDirectionCapacity.w;
    if (index >= capacity)
    {
        return;
    }

    particle_data particle = (particle_data)0;
    particle.parameter = float4(0.0f, 0.0f, 0.0f, 0.0f);
    particle.position = float4(gOriginTime.xyz, 1.0f);
    particle.rotation = float4(0.0f, 0.0f, 0.0f, 0.0f);
    particle.scale = float4(0.0f, 0.0f, 0.0f, 1.0f);
    particle.scale_begin = float4(gStartSize, gStartSize, gStartSize, 0.0f);
    particle.scale_end = float4(gEndSize, gEndSize, gEndSize, 0.0f);
    particle.velocity = float4(0.0f, 0.0f, 0.0f, 0.0f);
    particle.acceleration = float4(0.0f, -0.55f, 0.0f, 0.0f);
    particle.texcoord = float4(0.0f, 0.0f, 1.0f, 1.0f);
    particle.gradientCount = 0;
    particle.color = gTint;
    particle.angular_velocity = float4(0.0f, 0.0f, 0.0f, 0.0f);
    particle.fade = float2(0.0f, 1.0f);

    particle_header header;
    header.alive = 0;
    header.particle_index = index;
    header.depth = 0.0f;
    header.dummy = 0;

    const float duration = max(gTiming.x, 0.0f);
    const float spawnRate = max(gTiming.y, 0.0f);
    const float particleLifetime = max(gTiming.z, 0.001f);
    const float speed = max(gTiming.w, 0.0f);
    const float3 acceleration = gAccelerationDrag.xyz;
    const float drag = max(gAccelerationDrag.w, 0.0f);
    const uint shapeType = (uint)gShapeTypeSpin.x;
    const float spinRate = max(gShapeTypeSpin.y, 0.0f);
    const uint burstCount = (uint)gShapeTypeSpin.z;
    const float alphaCurveBias = max(gShapeTypeSpin.w, 0.05f);
    const float sizeCurveBias = max(gShapeParams.w, 0.05f);

    if (index < gMaxParticles && (spawnRate > 0.0f || burstCount > 0u))
    {
        uint emittedCount = min(gMaxParticles, burstCount);
        if (duration > 0.0f)
        {
            emittedCount = min(gMaxParticles, burstCount + (uint)ceil(duration * spawnRate));
        }
        else if (spawnRate > 0.0f)
        {
            emittedCount = gMaxParticles;
        }

        if (index < emittedCount)
        {
            float spawnTime = 0.0f;
            if (index >= burstCount)
            {
                spawnTime = (index - burstCount) / spawnRate;
            }
            const float age = gOriginTime.w - spawnTime;
            if (age >= 0.0f && age < particleLifetime)
            {
                const float normalizedAge = saturate(age / particleLifetime);
                const float sizeAge = saturate(pow(normalizedAge, sizeCurveBias));
                const float alphaAge = saturate(pow(normalizedAge, alphaCurveBias));
                const float fade = saturate(1.0f - alphaAge);

                const float rand0 = Hash01(index * 9781u + gSeed * 6271u + 17u);
                const float rand1 = Hash01(index * 6271u + gSeed * 9781u + 53u);
                const float rand2 = Hash01(index * 2371u + gSeed * 3253u + 97u);
                const float rand3 = Hash01(index * 7417u + gSeed * 1597u + 131u);
                const float rand4 = Hash01(index * 2137u + gSeed * 3571u + 193u);

                float3 spawnOffset = float3(0.0f, 0.0f, 0.0f);
                float3 direction = normalize(float3(0.0f, 1.0f, 0.0f));
                const float angle = rand0 * 6.2831853f;

                if (shapeType == 1u) // Sphere
                {
                    const float radius = max(gShapeParams.x, 0.001f);
                    spawnOffset = RandomDirection(index, gSeed) * radius * lerp(0.15f, 1.0f, rand1);
                    direction = normalize(spawnOffset + float3(0.0f, radius * 0.5f, 0.0f));
                }
                else if (shapeType == 2u) // Box
                {
                    const float3 extents = max(gShapeParams.xyz, float3(0.01f, 0.01f, 0.01f));
                    spawnOffset = float3(
                        lerp(-extents.x, extents.x, rand0),
                        lerp(-extents.y, extents.y, rand1),
                        lerp(-extents.z, extents.z, rand2));
                    direction = normalize(float3(spawnOffset.x, abs(spawnOffset.y) + 0.25f, spawnOffset.z));
                }
                else if (shapeType == 3u) // Cone
                {
                    const float angleDeg = max(gShapeParams.x, 1.0f);
                    const float baseRadius = max(gShapeParams.y, 0.05f);
                    const float coneHeight = max(gShapeParams.z, 0.05f);
                    const float coneAngle = radians(angleDeg);
                    const float coneRadius = tan(coneAngle) * coneHeight;
                    const float radius = lerp(baseRadius * 0.1f, coneRadius, rand1);
                    spawnOffset = float3(cos(angle) * radius, 0.0f, sin(angle) * radius);
                    direction = normalize(float3(cos(angle) * coneRadius, coneHeight, sin(angle) * coneRadius));
                }
                else if (shapeType == 4u) // Circle
                {
                    const float radius = max(gShapeParams.x, 0.05f);
                    spawnOffset = float3(cos(angle) * radius, 0.0f, sin(angle) * radius);
                    direction = normalize(float3(spawnOffset.x * 0.25f, 1.0f, spawnOffset.z * 0.25f));
                }
                else if (shapeType == 5u) // Line
                {
                    const float halfLength = max(gShapeParams.x, 0.05f);
                    spawnOffset = float3(lerp(-halfLength, halfLength, rand0), 0.0f, 0.0f);
                    direction = normalize(float3(0.0f, 1.0f, lerp(-0.35f, 0.35f, rand1)));
                }
                else // Point
                {
                    const float radial = lerp(0.2f, 0.85f, rand1);
                    const float lift = lerp(0.35f, 1.0f, rand2);
                    direction = normalize(float3(cos(angle) * radial, lift, sin(angle) * radial));
                }

                const float speedScale = lerp(0.65f, 1.35f, rand3);
                const float spin = lerp(-spinRate, spinRate, rand4);
                const float3 velocity = direction * (speed * speedScale);
                const float damping = max(0.0f, 1.0f - drag * normalizedAge);
                const float3 position = gOriginTime.xyz + spawnOffset + velocity * age * damping + 0.5f * acceleration * age * age;
                const float size = lerp(gStartSize, gEndSize, sizeAge);

                particle.position = float4(position, 1.0f);
                particle.rotation = float4(0.0f, 0.0f, spin * age, 0.0f);
                particle.scale = float4(size, size, size, 1.0f);
                particle.velocity = float4(velocity * damping, 0.0f);
                particle.acceleration = float4(acceleration, 0.0f);
                float4 lifeTint = lerp(gTint, gTintEnd, alphaAge);
                particle.color = float4(lifeTint.rgb, lifeTint.a * fade);
                particle.angular_velocity = float4(0.0f, 0.0f, spin, 0.0f);
                particle.fade = float2(1.0f - alphaAge, alphaAge);

                header.alive = 1;
                const float3 cameraToParticle = position - gCameraPositionSortSign.xyz;
                header.depth = dot(cameraToParticle, gCameraDirectionCapacity.xyz) * gCameraPositionSortSign.w;
            }
        }
    }

    particle_data_buffer[index] = particle;
    particle_header_buffer[index] = header;
}
