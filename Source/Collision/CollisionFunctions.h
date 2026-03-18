// CollisionFunctions.h
#pragma once
#include "Collision.h"

namespace CollisionFunctions
{
    bool IntersectSphereVsSphere(
        const DirectX::XMFLOAT3& positionA, float radiusA,
        const DirectX::XMFLOAT3& positionB, float radiusB,
        HitResult& hitResult);

    bool IntersectSphereVsCapsule(
        const DirectX::XMFLOAT3& positionSphere, float radiusSphere,
        const DirectX::XMFLOAT3& positionCapsule, float radiusCapsule, float heightCapsule,
        HitResult& hitResult);

    bool IntersectCapsuleVCapsule(
        const DirectX::XMFLOAT3& positionA, float radiusA, float heightA,
        const DirectX::XMFLOAT3& positionB, float radiusB, float heightB,
        HitResult& hitResult);

    bool IntersectSphereVsBox(
        const DirectX::XMFLOAT3& sphereCenter, float sphereRadius,
        const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize,
        HitResult& hitResult);

    // ★追加: Box vs Box (AABB)
    bool IntersectBoxVsBox(
        const DirectX::XMFLOAT3& boxACenter, const DirectX::XMFLOAT3& boxASize,
        const DirectX::XMFLOAT3& boxBCenter, const DirectX::XMFLOAT3& boxBSize,
        HitResult& hitResult);

    // レイ vs 球
    // t: 衝突までの距離（出力）
    bool IntersectRayVsSphere(
        const Ray& ray,
        const DirectX::XMFLOAT3& sphereCenter,
        float sphereRadius,
        float& t,
        DirectX::XMFLOAT3& outNormal);

    // レイ vs AABB (軸平行ボックス)
    bool IntersectRayVsBox(
        const Ray& ray,
        const DirectX::XMFLOAT3& boxCenter,
        const DirectX::XMFLOAT3& boxSize, // Full Size (幅・高さ・奥行き)
        float& t,
        DirectX::XMFLOAT3& outNormal);

    // レイ vs カプセル (Y軸平行)
    bool IntersectRayVsCapsule(
        const Ray& ray,
        const DirectX::XMFLOAT3& capsuleBase,
        float capsuleRadius,
        float capsuleHeight, // 円柱部分の高さ
        float& t,
        DirectX::XMFLOAT3& outNormal);

}
