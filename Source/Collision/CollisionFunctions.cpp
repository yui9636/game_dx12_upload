#include "CollisionFunctions.h"
#include <cmath>
#include <algorithm>

namespace {

    // 0.0f ～ 1.0f の範囲へ丸める。
    inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // 任意の最小値～最大値の範囲へ丸める。
    inline float Clamp(float v, float min, float max) { return (v < min) ? min : (v > max ? max : v); }

    // 浮動小数のゼロ比較用しきい値。
    constexpr float EPSILON = 1e-6f;
}

// ---------------------------------------------------------
// Sphere vs Sphere
// ---------------------------------------------------------
// 2つの球同士の当たり判定を行う。
// 重なっていれば hitResult に押し戻し結果も書き込む。
bool CollisionFunctions::IntersectSphereVsSphere(
    const DirectX::XMFLOAT3& positionA, float radiusA,
    const DirectX::XMFLOAT3& positionB, float radiusB,
    HitResult& hitResult)
{
    using namespace DirectX;

    // 半径が負にならないよう補正する。
    if (radiusA < 0.0f) radiusA = 0.0f;
    if (radiusB < 0.0f) radiusB = 0.0f;

    // A と B の中心座標をロードする。
    XMVECTOR a = XMLoadFloat3(&positionA);
    XMVECTOR b = XMLoadFloat3(&positionB);

    // A -> B ベクトルを求める。
    XMVECTOR d = XMVectorSubtract(b, a);

    // 中心間距離の二乗を求める。
    float dsq = XMVectorGetX(XMVector3Dot(d, d));

    // 当たり判定に使う合計半径。
    float rr = radiusA + radiusB;

    // 距離が合計半径より大きければ非接触。
    if (dsq > rr * rr) return false;

    // 実距離を求める。
    float dist = dsq > 0.0f ? std::sqrt(dsq) : 0.0f;

    // 接触位置はここでは B の中心を採用している。
    hitResult.hitPosition = { XMVectorGetX(b), XMVectorGetY(b), XMVectorGetZ(b) };

    // めり込み深さを求める。
    hitResult.penetrationDepth = rr - dist;

    // 押し戻し方向を作る。
    // 完全一致時は上方向を仮採用する。
    XMVECTOR n = (dist > 1e-6f) ? XMVectorScale(d, 1.0f / dist) : XMVectorSet(0, 1, 0, 0);

    // 半分ずつ押し戻す量を作る。
    XMVECTOR push = XMVectorScale(n, hitResult.penetrationDepth * 0.5f);

    // A は逆方向へ、B は順方向へ押し戻す。
    XMStoreFloat3(&hitResult.selfOutPosition, XMVectorSubtract(a, push));
    XMStoreFloat3(&hitResult.otherOutPosition, XMVectorAdd(b, push));
    return true;
}

// ---------------------------------------------------------
// Sphere vs Capsule
// ---------------------------------------------------------
// 球とカプセルの当たり判定を行う。
// カプセルは positionCapsule を底面中心、高さは +Y 方向へ伸びる前提。
bool CollisionFunctions::IntersectSphereVsCapsule(
    const DirectX::XMFLOAT3& positionSphere, float radiusSphere,
    const DirectX::XMFLOAT3& positionCapsule, float radiusCapsule, float heightCapsule,
    HitResult& hitResult)
{
    using namespace DirectX;

    // 半径と高さが負にならないよう補正する。
    if (radiusSphere < 0.0f) radiusSphere = 0.0f;
    if (radiusCapsule < 0.0f) radiusCapsule = 0.0f;
    if (heightCapsule < 0.0f) heightCapsule = 0.0f;

    // カプセル軸の始点 A と終点 B を作る。
    XMVECTOR A = XMLoadFloat3(&positionCapsule);
    XMVECTOR B = XMVectorAdd(A, XMVectorSet(0.0f, heightCapsule, 0.0f, 0.0f));

    // 球中心 C。
    XMVECTOR C = XMLoadFloat3(&positionSphere);

    // カプセル軸ベクトル。
    XMVECTOR AB = XMVectorSubtract(B, A);
    float ab2 = XMVectorGetX(XMVector3Dot(AB, AB));

    // 球中心をカプセル軸へ射影した係数 t を求める。
    float t = 0.0f;
    if (ab2 > 0.0f) {
        t = XMVectorGetX(XMVector3Dot(XMVectorSubtract(C, A), AB)) / ab2;
        t = Clamp01(t);
    }

    // カプセル軸上の最近傍点 P。
    XMVECTOR P = XMVectorAdd(A, XMVectorScale(AB, t));

    // カプセル最近傍点 -> 球中心ベクトル。
    XMVECTOR D = XMVectorSubtract(C, P);
    float dsq = XMVectorGetX(XMVector3Dot(D, D));

    // 当たり判定用の合計半径。
    float rr = radiusSphere + radiusCapsule;
    if (dsq > rr * rr) return false;

    // 実距離を求める。
    float dist = dsq > 0.0f ? std::sqrt(dsq) : 0.0f;

    // 接触点はカプセル軸上の最近傍点とする。
    hitResult.hitPosition = { XMVectorGetX(P), XMVectorGetY(P), XMVectorGetZ(P) };

    // めり込み深さを求める。
    hitResult.penetrationDepth = rr - dist;

    // 押し戻し方向を作る。
    XMVECTOR n = (dist > 1e-6f) ? XMVectorScale(D, 1.0f / dist) : XMVectorSet(0, 1, 0, 0);

    // 半分ずつ押し戻す量。
    XMVECTOR push = XMVectorScale(n, hitResult.penetrationDepth * 0.5f);

    // 球は外側へ、カプセルは逆方向へ押す。
    XMStoreFloat3(&hitResult.selfOutPosition, XMVectorAdd(C, push));
    XMStoreFloat3(&hitResult.otherOutPosition, XMVectorSubtract(A, push));

    return true;
}

// ---------------------------------------------------------
// Capsule vs Capsule
// ---------------------------------------------------------
// 2本のカプセル軸線分の最近傍点を求め、その距離で当たり判定する。
bool CollisionFunctions::IntersectCapsuleVCapsule(
    const DirectX::XMFLOAT3& positionA, float radiusA, float heightA,
    const DirectX::XMFLOAT3& positionB, float radiusB, float heightB,
    HitResult& hitResult)
{
    using namespace DirectX;

    // 半径と高さを負値から保護する。
    if (radiusA < 0.0f) radiusA = 0.0f;
    if (radiusB < 0.0f) radiusB = 0.0f;
    if (heightA < 0.0f) heightA = 0.0f;
    if (heightB < 0.0f) heightB = 0.0f;

    // A カプセル軸の始点・終点。
    XMVECTOR A0 = XMLoadFloat3(&positionA);
    XMVECTOR A1 = XMVectorAdd(A0, XMVectorSet(0.0f, heightA, 0.0f, 0.0f));

    // B カプセル軸の始点・終点。
    XMVECTOR B0 = XMLoadFloat3(&positionB);
    XMVECTOR B1 = XMVectorAdd(B0, XMVectorSet(0.0f, heightB, 0.0f, 0.0f));

    // 線分最近傍計算用ベクトル。
    XMVECTOR u = XMVectorSubtract(A1, A0);
    XMVECTOR v = XMVectorSubtract(B1, B0);
    XMVECTOR w = XMVectorSubtract(A0, B0);

    float a = XMVectorGetX(XMVector3Dot(u, u));
    float b = XMVectorGetX(XMVector3Dot(u, v));
    float c = XMVectorGetX(XMVector3Dot(v, v));
    float d = XMVectorGetX(XMVector3Dot(u, w));
    float e = XMVectorGetX(XMVector3Dot(v, w));

    // 2直線の最近傍パラメータを求める。
    float denom = a * c - b * b;
    float s, t;
    if (denom != 0.0f) {
        s = (b * e - c * d) / denom;
        t = (a * e - b * d) / denom;
    }
    else {
        // 平行に近い時は片方を 0 に固定して近似する。
        s = (a > 0.0f) ? (-d / a) : 0.0f;
        t = 0.0f;
    }

    // 線分範囲へ丸める。
    if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

    // 各線分上の最近傍点。
    XMVECTOR PA = XMVectorAdd(A0, XMVectorScale(u, s));
    XMVECTOR PB = XMVectorAdd(B0, XMVectorScale(v, t));

    // B -> A ベクトル。
    XMVECTOR dP = XMVectorSubtract(PA, PB);
    float dsq = XMVectorGetX(XMVector3Dot(dP, dP));

    // 合計半径。
    float rr = radiusA + radiusB;
    if (dsq > rr * rr) return false;

    float dist = dsq > 0.0f ? std::sqrt(dsq) : 0.0f;

    // 接触位置は B 側最近傍点を採用。
    hitResult.hitPosition = { XMVectorGetX(PB), XMVectorGetY(PB), XMVectorGetZ(PB) };

    // めり込み深さ。
    hitResult.penetrationDepth = rr - dist;

    // 押し戻し方向。
    XMVECTOR n = (dist > 1e-6f) ? XMVectorScale(dP, 1.0f / dist) : XMVectorSet(0, 1, 0, 0);
    XMVECTOR push = XMVectorScale(n, hitResult.penetrationDepth * 0.5f);

    // 両者を半分ずつ押し戻す。
    XMStoreFloat3(&hitResult.selfOutPosition, XMVectorAdd(A0, push));
    XMStoreFloat3(&hitResult.otherOutPosition, XMVectorSubtract(B0, push));

    return true;
}

// ---------------------------------------------------------
// Sphere vs Box
// ---------------------------------------------------------
// 球と AABB の当たり判定を行う。
bool CollisionFunctions::IntersectSphereVsBox(
    const DirectX::XMFLOAT3& sphereCenter, float sphereRadius,
    const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize,
    HitResult& hitResult)
{
    using namespace DirectX;

    // Box 中心と half size を作る。
    XMVECTOR bCenter = XMLoadFloat3(&boxCenter);
    XMVECTOR bHalf = XMLoadFloat3(&boxSize) * 0.5f;
    XMVECTOR bMin = bCenter - bHalf;
    XMVECTOR bMax = bCenter + bHalf;

    // 球中心。
    XMVECTOR sCenter = XMLoadFloat3(&sphereCenter);

    // 球中心を AABB 内へ clamp した最近傍点を求める。
    XMVECTOR closestPoint = XMVectorMin(XMVectorMax(sCenter, bMin), bMax);

    // 最近傍点から球中心へのベクトル。
    XMVECTOR distVec = closestPoint - sCenter;
    float distSq = XMVectorGetX(XMVector3Dot(distVec, distVec));

    // 半径外なら非接触。
    if (distSq > sphereRadius * sphereRadius) return false;

    float dist = std::sqrt(distSq);

    // 接触点は最近傍点。
    hitResult.hitPosition = { XMVectorGetX(closestPoint), XMVectorGetY(closestPoint), XMVectorGetZ(closestPoint) };

    XMVECTOR normal;
    float depth;

    if (dist > 1e-6f)
    {
        // 外部から接触した通常ケース。
        normal = distVec * (1.0f / dist);
        depth = sphereRadius - dist;
    }
    else
    {
        // 球中心が箱内部にあるケース。
        normal = XMVectorSet(0, 1, 0, 0);
        depth = sphereRadius;
    }

    hitResult.penetrationDepth = depth;
    XMVECTOR push = normal * (depth * 0.5f);

    // 球は逆方向へ、箱は順方向へ押し戻す。
    XMStoreFloat3(&hitResult.selfOutPosition, sCenter - push);
    XMStoreFloat3(&hitResult.otherOutPosition, bCenter + push);

    return true;
}

// ---------------------------------------------------------
// Box vs Box
// ---------------------------------------------------------
// 2つの AABB の重なりを軸ごとに調べ、最小めり込み軸で押し戻す。
bool CollisionFunctions::IntersectBoxVsBox(
    const DirectX::XMFLOAT3& boxACenter, const DirectX::XMFLOAT3& boxASize,
    const DirectX::XMFLOAT3& boxBCenter, const DirectX::XMFLOAT3& boxBSize,
    HitResult& hitResult)
{
    using namespace DirectX;

    // 各軸の中心差。
    float dx = std::abs(boxACenter.x - boxBCenter.x);
    float dy = std::abs(boxACenter.y - boxBCenter.y);
    float dz = std::abs(boxACenter.z - boxBCenter.z);

    // 各軸の half size 合計。
    float sumHalfX = (boxASize.x + boxBSize.x) * 0.5f;
    float sumHalfY = (boxASize.y + boxBSize.y) * 0.5f;
    float sumHalfZ = (boxASize.z + boxBSize.z) * 0.5f;

    // どれか1軸でも離れていれば非接触。
    if (dx >= sumHalfX || dy >= sumHalfY || dz >= sumHalfZ) return false;

    // 各軸のめり込み量。
    float penX = sumHalfX - dx;
    float penY = sumHalfY - dy;
    float penZ = sumHalfZ - dz;

    // 最小めり込み軸を選ぶ。
    float minPen = penX;
    int axis = 0; // 0:x, 1:y, 2:z
    if (penY < minPen) { minPen = penY; axis = 1; }
    if (penZ < minPen) { minPen = penZ; axis = 2; }

    hitResult.penetrationDepth = minPen;

    // 押し戻し法線を最小軸に沿って決める。
    XMVECTOR n = XMVectorZero();
    if (axis == 0) n = XMVectorSet(boxACenter.x > boxBCenter.x ? 1.0f : -1.0f, 0, 0, 0);
    else if (axis == 1) n = XMVectorSet(0, boxACenter.y > boxBCenter.y ? 1.0f : -1.0f, 0, 0);
    else n = XMVectorSet(0, 0, boxACenter.z > boxBCenter.z ? 1.0f : -1.0f, 0);

    XMVECTOR push = n * (minPen * 0.5f);
    XMVECTOR posA = XMLoadFloat3(&boxACenter);
    XMVECTOR posB = XMLoadFloat3(&boxBCenter);

    // 接触位置は中点で近似する。
    XMVECTOR contact = (posA + posB) * 0.5f;
    XMStoreFloat3(&hitResult.hitPosition, contact);

    // 半分ずつ押し戻す。
    XMStoreFloat3(&hitResult.selfOutPosition, posA + push);
    XMStoreFloat3(&hitResult.otherOutPosition, posB - push);

    return true;
}

// ---------------------------------------------------------
// Ray vs Sphere
// ---------------------------------------------------------
// レイと球の交差判定を行う。
// 命中時は距離 t と法線を返す。
bool CollisionFunctions::IntersectRayVsSphere(
    const Ray& ray,
    const DirectX::XMFLOAT3& sphereCenter,
    float sphereRadius,
    float& outT,
    DirectX::XMFLOAT3& outNormal)
{
    using namespace DirectX;

    // レイ原点 O、方向 D、球中心 C。
    XMVECTOR O = XMLoadFloat3(&ray.origin);
    XMVECTOR D = XMLoadFloat3(&ray.direction);
    XMVECTOR C = XMLoadFloat3(&sphereCenter);

    // 球中心からレイ原点へのベクトル。
    XMVECTOR M = O - C;

    float b = XMVectorGetX(XMVector3Dot(M, D));
    float c = XMVectorGetX(XMVector3Dot(M, M)) - (sphereRadius * sphereRadius);

    // レイ原点が球外かつ進行方向が外向きならヒットしない。
    if (c > 0.0f && b > 0.0f) return false;

    // 判別式。
    float discr = b * b - c;
    if (discr < 0.0f) return false;

    // 最も近い交差距離。
    float t = -b - sqrtf(discr);

    // 球内部から始まる時は 0 とする。
    if (t < 0.0f) t = 0.0f;

    outT = t;

    // ヒット位置から球中心へ向かう法線を作る。
    XMVECTOR HitPoint = O + D * t;
    XMVECTOR Normal = XMVector3Normalize(HitPoint - C);
    XMStoreFloat3(&outNormal, Normal);

    return true;
}

// ---------------------------------------------------------
// Ray vs Box
// ---------------------------------------------------------
// レイと AABB の交差判定をスラブ法で行う。
bool CollisionFunctions::IntersectRayVsBox(
    const Ray& ray,
    const DirectX::XMFLOAT3& boxCenter,
    const DirectX::XMFLOAT3& boxSize,
    float& outT,
    DirectX::XMFLOAT3& outNormal)
{
    using namespace DirectX;

    // AABB の min / max を求める。
    XMVECTOR Center = XMLoadFloat3(&boxCenter);
    XMVECTOR Size = XMLoadFloat3(&boxSize);
    XMVECTOR HalfSize = Size * 0.5f;
    XMVECTOR Min = Center - HalfSize;
    XMVECTOR Max = Center + HalfSize;

    // レイ原点と方向。
    XMVECTOR O = XMLoadFloat3(&ray.origin);
    XMVECTOR D = XMLoadFloat3(&ray.direction);

    // 各軸アクセスしやすいよう一旦配列へ落とす。
    float o[3], d[3], min[3], max[3];
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(o), O);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(d), D);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(min), Min);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(max), Max);

    float tMin = 0.0f;
    float tMax = FLT_MAX;

    // 最終的なヒット面法線用。
    int axisSign[3] = { 0, 0, 0 };

    for (int i = 0; i < 3; ++i)
    {
        // ほぼ平行なら、その軸で箱範囲内に居るかだけを見る。
        if (fabsf(d[i]) < EPSILON)
        {
            if (o[i] < min[i] || o[i] > max[i]) return false;
        }
        else
        {
            float invD = 1.0f / d[i];
            float t1 = (min[i] - o[i]) * invD;
            float t2 = (max[i] - o[i]) * invD;

            // 小さい方を入口、大きい方を出口にそろえる。
            if (t1 > t2) std::swap(t1, t2);

            // 一番遅い入口を更新した時、その面法線を記録する。
            if (t1 > tMin)
            {
                tMin = t1;
                axisSign[0] = (i == 0) ? (invD < 0 ? 1 : -1) : 0;
                axisSign[1] = (i == 1) ? (invD < 0 ? 1 : -1) : 0;
                axisSign[2] = (i == 2) ? (invD < 0 ? 1 : -1) : 0;
            }

            // 一番早い出口を更新する。
            if (t2 < tMax) tMax = t2;

            // 入口が出口を超えたら交差しない。
            if (tMin > tMax) return false;
        }
    }

    outT = tMin;
    outNormal = DirectX::XMFLOAT3((float)axisSign[0], (float)axisSign[1], (float)axisSign[2]);
    return true;
}

// ---------------------------------------------------------
// Ray vs Capsule
// ---------------------------------------------------------
// レイと縦向きカプセルの交差判定を行う。
// 円柱部分と上下の半球を別々に調べ、最も近いヒットを採用する。
bool CollisionFunctions::IntersectRayVsCapsule(
    const Ray& ray,
    const DirectX::XMFLOAT3& capsuleBase,
    float radius,
    float height,
    float& outT,
    DirectX::XMFLOAT3& outNormal)
{
    using namespace DirectX;

    // カプセル底面中心と軸上端。
    XMVECTOR Base = XMLoadFloat3(&capsuleBase);
    XMVECTOR Axis = XMVectorSet(0, 1, 0, 0);
    XMVECTOR Top = Base + Axis * height;

    // レイ原点と方向。
    XMVECTOR O = XMLoadFloat3(&ray.origin);
    XMVECTOR D = XMLoadFloat3(&ray.direction);

    // 最も近いヒット候補。
    float closestT = FLT_MAX;
    XMVECTOR closestNormal = XMVectorZero();
    bool hit = false;

    // 円柱部分判定用に、Base 基準へ移す。
    XMVECTOR RC = O - Base;

    float ox = XMVectorGetX(RC); float oz = XMVectorGetZ(RC);
    float dx = XMVectorGetX(D);  float dz = XMVectorGetZ(D);

    // XZ 平面上で無限円柱との交差を解く。
    float a = dx * dx + dz * dz;
    float b = 2.0f * (ox * dx + oz * dz);
    float c = (ox * ox + oz * oz) - radius * radius;

    if (a > EPSILON)
    {
        float discr = b * b - 4.0f * a * c;
        if (discr >= 0.0f)
        {
            float sq = sqrtf(discr);
            float t1 = (-b - sq) / (2.0f * a);

            if (t1 > 0.0f)
            {
                // ヒット位置の y がカプセルの高さ範囲に入っているか確認する。
                float y = XMVectorGetY(O) + XMVectorGetY(D) * t1;
                float baseY = XMVectorGetY(Base);

                if (y >= baseY && y <= baseY + height)
                {
                    closestT = t1;
                    hit = true;

                    XMVECTOR HitP = O + D * t1;

                    // 円柱軸上の最近傍点を使って法線を求める。
                    XMVECTOR N = HitP - XMVectorSet(XMVectorGetX(HitP), y, XMVectorGetZ(HitP), 0);
                    XMVECTOR AxisPt = Base + Axis * (y - baseY);
                    closestNormal = XMVector3Normalize(HitP - AxisPt);
                }
            }
        }
    }

    // 下側半球との交差判定。
    float tSphere;
    DirectX::XMFLOAT3 nSphere;
    if (IntersectRayVsSphere(ray, capsuleBase, radius, tSphere, nSphere))
    {
        if (tSphere < closestT)
        {
            closestT = tSphere;
            closestNormal = XMLoadFloat3(&nSphere);
            hit = true;
        }
    }

    // 上側半球との交差判定。
    DirectX::XMFLOAT3 topPos;
    XMStoreFloat3(&topPos, Top);
    if (IntersectRayVsSphere(ray, topPos, radius, tSphere, nSphere))
    {
        if (tSphere < closestT)
        {
            closestT = tSphere;
            closestNormal = XMLoadFloat3(&nSphere);
            hit = true;
        }
    }

    // どこかに当たっていれば最も近い結果を返す。
    if (hit)
    {
        outT = closestT;
        XMStoreFloat3(&outNormal, closestNormal);
        return true;
    }

    return false;
}