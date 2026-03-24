#include "ParticleUtils.h"
#include <cmath>

using namespace DirectX;

float ParticleUtils::Random01(std::mt19937& rng)
{
    std::uniform_real_distribution<float> d(0.0f, 1.0f);
    return d(rng);
}


namespace {
    XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) { return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z); }
    XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z); }
    XMFLOAT3 Mul(const XMFLOAT3& v, float s) { return XMFLOAT3(v.x * s, v.y * s, v.z * s); }
    float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    XMFLOAT3 NormalizeSafe(const XMFLOAT3& v, const XMFLOAT3& fallback) {
        XMVECTOR vv = XMLoadFloat3(&v);
        XMVECTOR dot = XMVector3Dot(vv, vv);
        float lenSq = XMVectorGetX(dot);
        if (lenSq <= 1.0e-12f) return fallback;
        XMFLOAT3 out;
        XMStoreFloat3(&out, XMVector3Normalize(vv));
        return out;
    }

    XMFLOAT3 RandomUnitVector(std::mt19937& rng) {
        float u = ParticleUtils::Random01(rng);
        float v = ParticleUtils::Random01(rng);
        float theta = 6.28318530718f * u;
        float z = 2.0f * v - 1.0f;
        float r = std::sqrt(1.0f - z * z);
        return XMFLOAT3(r * std::cos(theta), r * std::sin(theta), z);
    }
}

float ParticleUtils::RandomRange(std::mt19937& rng, float min, float max) {
    std::uniform_real_distribution<float> d(min, max);
    return d(rng);
}


XMFLOAT3 ParticleUtils::RotateVector(const XMFLOAT3& v, const XMFLOAT4& q) {
    XMVECTOR V = XMLoadFloat3(&v);
    XMVECTOR Q = XMLoadFloat4(&q);
    XMFLOAT3 out;
    XMStoreFloat3(&out, XMVector3Rotate(V, Q));
    return out;
}

XMFLOAT3 ParticleUtils::SampleEmissionPosition(const ParticleSetting& e, std::mt19937& rng)
{
    switch (e.shape)
    {
    case ShapeType::Point: return XMFLOAT3(0, 0, 0);
    case ShapeType::Sphere: {
        XMFLOAT3 dir = RandomUnitVector(rng);
        if (e.surfaceOnly) return Mul(dir, e.radius);
        float r = std::pow(Random01(rng), 1.0f / 3.0f) * e.radius;
        return Mul(dir, r);
    }
    case ShapeType::Box: {
        XMFLOAT3 half(e.boxSize.x * 0.5f, e.boxSize.y * 0.5f, e.boxSize.z * 0.5f);
        if (!e.surfaceOnly) {
            return XMFLOAT3(RandomRange(rng, -half.x, half.x), RandomRange(rng, -half.y, half.y), RandomRange(rng, -half.z, half.z));
        }
        int face = (int)(Random01(rng) * 6.0f);
        float u = RandomRange(rng, -1.0f, 1.0f);
        float v = RandomRange(rng, -1.0f, 1.0f);
        if (face == 0) return XMFLOAT3(half.x, u * half.y, v * half.z);
        else if (face == 1) return XMFLOAT3(-half.x, u * half.y, v * half.z);
        else if (face == 2) return XMFLOAT3(u * half.x, half.y, v * half.z);
        else if (face == 3) return XMFLOAT3(u * half.x, -half.y, v * half.z);
        else if (face == 4) return XMFLOAT3(u * half.x, v * half.y, half.z);
        else return XMFLOAT3(u * half.x, v * half.y, -half.z);
    }
    case ShapeType::Cone: return XMFLOAT3(0, 0, 0); // Cone starts at origin
    case ShapeType::Spark: return XMFLOAT3(0, 0, 0);
    case ShapeType::Circle: {
        float angle = RandomRange(rng, 0.0f, 6.2831853f);
        float r = e.circleRadius;
        if (!e.surfaceOnly) r *= std::sqrt(Random01(rng));
        return XMFLOAT3(std::cos(angle) * r, std::sin(angle) * r, 0.0f);
    }
    default: return XMFLOAT3(0, 0, 0);
    }
}

XMFLOAT3 ParticleUtils::SampleEmissionDirection(const ParticleSetting& e, const XMFLOAT3& localPos, std::mt19937& rng)
{
    if (e.shape == ShapeType::Cone || e.shape == ShapeType::Spark) {
        float thetaMax = XMConvertToRadians(e.coneAngleDeg);
        float u = Random01(rng);
        float v = Random01(rng);
        float cosTheta = Lerp(std::cos(thetaMax), 1.0f, u);
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
        float phi = 6.2831853f * v;

        XMFLOAT3 z = NormalizeSafe(e.coneDirection, XMFLOAT3(0, 1, 0));
        XMFLOAT3 tmp = (std::abs(z.z) > 0.999f) ? XMFLOAT3(1, 0, 0) : XMFLOAT3(0, 0, 1);
        XMVECTOR Z = XMLoadFloat3(&z);
        XMVECTOR T = XMLoadFloat3(&tmp);
        XMVECTOR X = XMVector3Normalize(XMVector3Cross(T, Z));
        XMVECTOR Y = XMVector3Cross(Z, X);
        XMFLOAT3 x, y; XMStoreFloat3(&x, X); XMStoreFloat3(&y, Y);

        return XMFLOAT3(
            x.x * sinTheta * std::cos(phi) + y.x * sinTheta * std::sin(phi) + z.x * cosTheta,
            x.y * sinTheta * std::cos(phi) + y.y * sinTheta * std::sin(phi) + z.y * cosTheta,
            x.z * sinTheta * std::cos(phi) + y.z * sinTheta * std::sin(phi) + z.z * cosTheta
        );
    }

    // Radial
    if (e.shape == ShapeType::Sphere || e.shape == ShapeType::Hemisphere || e.shape == ShapeType::Circle) {
        return NormalizeSafe(localPos, XMFLOAT3(0, 0, 1));
    }
    return XMFLOAT3(0, 0, 1); // Default Z+
}

XMFLOAT3 ParticleUtils::ComputeVelocity(const ParticleSetting& e, const XMFLOAT3& directionLocal, const XMFLOAT4& worldRotation, std::mt19937& rng)
{
    bool hasVelRange = (e.minVelocity.x != e.maxVelocity.x || e.minVelocity.y != e.maxVelocity.y || e.minVelocity.z != e.maxVelocity.z);
    if (hasVelRange) {
        return XMFLOAT3(
            RandomRange(rng, e.minVelocity.x, e.maxVelocity.x),
            RandomRange(rng, e.minVelocity.y, e.maxVelocity.y),
            RandomRange(rng, e.minVelocity.z, e.maxVelocity.z)
        );
    }
    float speed = RandomRange(rng, e.minSpeed, e.maxSpeed);
    XMFLOAT3 dirWorld = RotateVector(directionLocal, worldRotation);
    return Mul(dirWorld, speed);
}
