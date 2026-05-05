#pragma once
#include <DirectXMath.h>
#include <random>
#include "ParticleSetting.h"

// パーティクルの発生位置・発生方向・初速を計算する補助クラス。
class ParticleUtils
{
public:
    // 設定された発生形状から、ローカル空間の発生位置をランダムに求める。
    static DirectX::XMFLOAT3 SampleEmissionPosition(
        const ParticleSetting& settings,
        std::mt19937& rng
    );

    // 発生形状と発生位置から、ローカル空間の発射方向を求める。
    static DirectX::XMFLOAT3 SampleEmissionDirection(
        const ParticleSetting& settings,
        const DirectX::XMFLOAT3& localPos,
        std::mt19937& rng
    );

    // ローカル方向・ワールド回転・速度設定から、最終的な初速ベクトルを求める。
    static DirectX::XMFLOAT3 ComputeVelocity(
        const ParticleSetting& settings,
        const DirectX::XMFLOAT3& directionLocal,
        const DirectX::XMFLOAT4& worldRotation,
        std::mt19937& rng
    );

    // クォータニオンでベクトルを回転する。
    static DirectX::XMFLOAT3 RotateVector(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT4& q);

    // min～max の範囲でランダムな float 値を返す。
    static float RandomRange(std::mt19937& rng, float min, float max);

    // 0.0～1.0 の範囲でランダムな float 値を返す。
    static float Random01(std::mt19937& rng);
};
