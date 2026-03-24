#pragma once
#include <DirectXMath.h>
#include <random>
#include "ParticleSetting.h"

class ParticleUtils
{
public:
    static DirectX::XMFLOAT3 SampleEmissionPosition(
        const ParticleSetting& settings,
        std::mt19937& rng
    );

    static DirectX::XMFLOAT3 SampleEmissionDirection(
        const ParticleSetting& settings,
        const DirectX::XMFLOAT3& localPos,
        std::mt19937& rng
    );

    static DirectX::XMFLOAT3 ComputeVelocity(
        const ParticleSetting& settings,
        const DirectX::XMFLOAT3& directionLocal,
        const DirectX::XMFLOAT4& worldRotation,
        std::mt19937& rng
    );

    static DirectX::XMFLOAT3 RotateVector(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT4& q);
    static float RandomRange(std::mt19937& rng, float min, float max);
    static float Random01(std::mt19937& rng);
};
