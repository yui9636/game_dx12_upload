#pragma once
#include <DirectXMath.h>
#include <random>
#include "ParticleSetting.h"

class ParticleUtils
{
public:
    // 形状に基づき、ローカル座標をランダム生成
    static DirectX::XMFLOAT3 SampleEmissionPosition(
        const ParticleSetting& settings,
        std::mt19937& rng
    );

    // 形状に基づき、射出方向ベクトルを生成
    static DirectX::XMFLOAT3 SampleEmissionDirection(
        const ParticleSetting& settings,
        const DirectX::XMFLOAT3& localPos,
        std::mt19937& rng
    );

    // 設定に基づき、速度ベクトル(World)を計算
    // worldRotation: エミッター自体の回転
    static DirectX::XMFLOAT3 ComputeVelocity(
        const ParticleSetting& settings,
        const DirectX::XMFLOAT3& directionLocal,
        const DirectX::XMFLOAT4& worldRotation,
        std::mt19937& rng
    );

    // ユーティリティ
    static DirectX::XMFLOAT3 RotateVector(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT4& q);
    static float RandomRange(std::mt19937& rng, float min, float max);
    static float Random01(std::mt19937& rng);
};