#include "ParticleUtils.h"
#include <cmath>

using namespace DirectX;

// 0.0～1.0 のランダム値を返す。
float ParticleUtils::Random01(std::mt19937& rng)
{
    std::uniform_real_distribution<float> d(0.0f, 1.0f);
    return d(rng);
}

namespace {
    // 2つの XMFLOAT3 を加算する。
    XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) { return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z); }

    // 2つの XMFLOAT3 を減算する。
    XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z); }

    // XMFLOAT3 にスカラーを掛ける。
    XMFLOAT3 Mul(const XMFLOAT3& v, float s) { return XMFLOAT3(v.x * s, v.y * s, v.z * s); }

    // a から b へ t の割合で線形補間する。
    float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    // 長さがほぼ 0 の場合は fallback を返し、それ以外は正規化したベクトルを返す。
    XMFLOAT3 NormalizeSafe(const XMFLOAT3& v, const XMFLOAT3& fallback) {
        XMVECTOR vv = XMLoadFloat3(&v);
        XMVECTOR dot = XMVector3Dot(vv, vv);
        float lenSq = XMVectorGetX(dot);
        if (lenSq <= 1.0e-12f) return fallback;
        XMFLOAT3 out;
        XMStoreFloat3(&out, XMVector3Normalize(vv));
        return out;
    }

    // 球面上のランダムな単位方向ベクトルを返す。
    XMFLOAT3 RandomUnitVector(std::mt19937& rng) {
        float u = ParticleUtils::Random01(rng);
        float v = ParticleUtils::Random01(rng);
        float theta = 6.28318530718f * u;
        float z = 2.0f * v - 1.0f;
        float r = std::sqrt(1.0f - z * z);
        return XMFLOAT3(r * std::cos(theta), r * std::sin(theta), z);
    }
}

// min～max の範囲でランダム値を返す。
float ParticleUtils::RandomRange(std::mt19937& rng, float min, float max) {
    std::uniform_real_distribution<float> d(min, max);
    return d(rng);
}

// クォータニオン q でベクトル v を回転する。
XMFLOAT3 ParticleUtils::RotateVector(const XMFLOAT3& v, const XMFLOAT4& q) {
    XMVECTOR V = XMLoadFloat3(&v);
    XMVECTOR Q = XMLoadFloat4(&q);
    XMFLOAT3 out;
    XMStoreFloat3(&out, XMVector3Rotate(V, Q));
    return out;
}

// 設定された形状に応じて、パーティクルのローカル発生位置をサンプリングする。
XMFLOAT3 ParticleUtils::SampleEmissionPosition(const ParticleSetting& e, std::mt19937& rng)
{
    switch (e.shape)
    {
    case ShapeType::Point:
        // 点形状は常に原点から発生する。
        return XMFLOAT3(0, 0, 0);
    case ShapeType::Sphere: {
        // 球形状はランダム方向を取り、surfaceOnly なら表面、そうでなければ球内部から発生する。
        XMFLOAT3 dir = RandomUnitVector(rng);
        if (e.surfaceOnly) return Mul(dir, e.radius);
        float r = std::pow(Random01(rng), 1.0f / 3.0f) * e.radius;
        return Mul(dir, r);
    }
    case ShapeType::Box: {
        // Box は中心基準の半サイズを使い、内部または面上からランダムに発生する。
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
    case ShapeType::Cone:
        // Cone は位置ではなく方向側で広がりを表現するため、発生位置は原点にする。
        return XMFLOAT3(0, 0, 0);
    case ShapeType::Spark:
        // Spark も Cone と同じく方向側で火花の広がりを表現する。
        return XMFLOAT3(0, 0, 0);
    case ShapeType::Circle: {
        // Circle は XY 平面上の円内または円周上から発生する。
        float angle = RandomRange(rng, 0.0f, 6.2831853f);
        float r = e.circleRadius;
        if (!e.surfaceOnly) r *= std::sqrt(Random01(rng));
        return XMFLOAT3(std::cos(angle) * r, std::sin(angle) * r, 0.0f);
    }
    default:
        // 未実装形状は安全側で原点発生にする。
        return XMFLOAT3(0, 0, 0);
    }
}

// 発生形状とローカル位置から、パーティクルのローカル発射方向をサンプリングする。
XMFLOAT3 ParticleUtils::SampleEmissionDirection(const ParticleSetting& e, const XMFLOAT3& localPos, std::mt19937& rng)
{
    if (e.shape == ShapeType::Cone || e.shape == ShapeType::Spark) {
        // Cone / Spark は coneDirection を中心軸として、coneAngleDeg の範囲内に方向を散らす。
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

    // Sphere / Hemisphere / Circle は発生位置から外側へ向かう放射方向を使う。
    if (e.shape == ShapeType::Sphere || e.shape == ShapeType::Hemisphere || e.shape == ShapeType::Circle) {
        return NormalizeSafe(localPos, XMFLOAT3(0, 0, 1));
    }

    // それ以外の形状は標準方向として Z+ を使う。
    return XMFLOAT3(0, 0, 1);
}

// 速度範囲または方向と速度設定から、パーティクルの初速を計算する。
XMFLOAT3 ParticleUtils::ComputeVelocity(const ParticleSetting& e, const XMFLOAT3& directionLocal, const XMFLOAT4& worldRotation, std::mt19937& rng)
{
    bool hasVelRange = (e.minVelocity.x != e.maxVelocity.x || e.minVelocity.y != e.maxVelocity.y || e.minVelocity.z != e.maxVelocity.z);
    if (hasVelRange) {
        // 成分ごとの速度範囲が指定されている場合は、その値を優先してランダム速度を作る。
        return XMFLOAT3(
            RandomRange(rng, e.minVelocity.x, e.maxVelocity.x),
            RandomRange(rng, e.minVelocity.y, e.maxVelocity.y),
            RandomRange(rng, e.minVelocity.z, e.maxVelocity.z)
        );
    }

    // 方向指定型の場合は、ローカル方向をワールド回転で回してから速度を掛ける。
    float speed = RandomRange(rng, e.minSpeed, e.maxSpeed);
    XMFLOAT3 dirWorld = RotateVector(directionLocal, worldRotation);
    return Mul(dirWorld, speed);
}
