// CollisionFunctions.h
#pragma once
#include "Collision.h"

// 各種コリジョン形状どうしの当たり判定関数群。
// 重なり判定だけでなく、必要に応じて押し戻し結果やヒット情報も返す。
namespace CollisionFunctions
{
    // 球 vs 球 の当たり判定を行う。
    // 接触していれば hitResult に接触位置、めり込み深さ、押し戻し位置を書き込む。
    bool IntersectSphereVsSphere(
        const DirectX::XMFLOAT3& positionA, float radiusA,
        const DirectX::XMFLOAT3& positionB, float radiusB,
        HitResult& hitResult);

    // 球 vs カプセル の当たり判定を行う。
    // カプセルは positionCapsule を底面中心とし、+Y 方向へ heightCapsule だけ伸びる前提。
    bool IntersectSphereVsCapsule(
        const DirectX::XMFLOAT3& positionSphere, float radiusSphere,
        const DirectX::XMFLOAT3& positionCapsule, float radiusCapsule, float heightCapsule,
        HitResult& hitResult);

    // カプセル vs カプセル の当たり判定を行う。
    // 両カプセルとも底面中心 +Y 方向へ高さが伸びる前提。
    bool IntersectCapsuleVCapsule(
        const DirectX::XMFLOAT3& positionA, float radiusA, float heightA,
        const DirectX::XMFLOAT3& positionB, float radiusB, float heightB,
        HitResult& hitResult);

    // 球 vs AABB の当たり判定を行う。
    // boxCenter は箱中心、boxSize は各軸の全体サイズ。
    bool IntersectSphereVsBox(
        const DirectX::XMFLOAT3& sphereCenter, float sphereRadius,
        const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize,
        HitResult& hitResult);

    // AABB vs AABB の当たり判定を行う。
    bool IntersectBoxVsBox(
        const DirectX::XMFLOAT3& boxACenter, const DirectX::XMFLOAT3& boxASize,
        const DirectX::XMFLOAT3& boxBCenter, const DirectX::XMFLOAT3& boxBSize,
        HitResult& hitResult);

    // レイ vs 球 の交差判定を行う。
    // 命中時はヒット距離 t とヒット面法線を返す。
    bool IntersectRayVsSphere(
        const Ray& ray,
        const DirectX::XMFLOAT3& sphereCenter,
        float sphereRadius,
        float& t,
        DirectX::XMFLOAT3& outNormal);

    // レイ vs AABB の交差判定を行う。
    // 命中時はヒット距離 t とヒット面法線を返す。
    bool IntersectRayVsBox(
        const Ray& ray,
        const DirectX::XMFLOAT3& boxCenter,
        const DirectX::XMFLOAT3& boxSize,
        float& t,
        DirectX::XMFLOAT3& outNormal);

    // レイ vs カプセル の交差判定を行う。
    // カプセルは底面中心 +Y 方向へ高さが伸びる前提。
    // 命中時は最も近いヒット距離 t と法線を返す。
    bool IntersectRayVsCapsule(
        const Ray& ray,
        const DirectX::XMFLOAT3& capsuleBase,
        float capsuleRadius,
        float capsuleHeight,
        float& t,
        DirectX::XMFLOAT3& outNormal);

}