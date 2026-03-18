#include "CollisionFunctions.h"
#include <cmath>
#include <algorithm>
namespace { 
    
    inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); } 
    inline float Clamp(float v, float min, float max) { return (v < min) ? min : (v > max ? max : v); }
    constexpr float EPSILON = 1e-6f;
}

bool CollisionFunctions::IntersectSphereVsSphere(
    const DirectX::XMFLOAT3& positionA, float radiusA,
    const DirectX::XMFLOAT3& positionB, float radiusB,
    HitResult& hitResult)
{
    using namespace DirectX;
    if (radiusA < 0.0f) radiusA = 0.0f; if (radiusB < 0.0f) radiusB = 0.0f;

    XMVECTOR a = XMLoadFloat3(&positionA);
    XMVECTOR b = XMLoadFloat3(&positionB);
    XMVECTOR d = XMVectorSubtract(b, a); // A -> B
    float dsq = XMVectorGetX(XMVector3Dot(d, d));
    float rr = radiusA + radiusB;
    if (dsq > rr * rr) return false;

    float dist = dsq > 0.0f ? std::sqrt(dsq) : 0.0f;
    hitResult.hitPosition = { XMVectorGetX(b), XMVectorGetY(b), XMVectorGetZ(b) };
    hitResult.penetrationDepth = rr - dist;

    XMVECTOR n = (dist > 1e-6f) ? XMVectorScale(d, 1.0f / dist) : XMVectorSet(0, 1, 0, 0); // A -> B
    XMVECTOR push = XMVectorScale(n, hitResult.penetrationDepth * 0.5f);

    // AはBと逆方向へ (-n)、BはAと逆方向へ (+n)
    XMStoreFloat3(&hitResult.selfOutPosition, XMVectorSubtract(a, push));
    XMStoreFloat3(&hitResult.otherOutPosition, XMVectorAdd(b, push));
    return true;
}

bool CollisionFunctions::IntersectSphereVsCapsule(
    const DirectX::XMFLOAT3& positionSphere, float radiusSphere,
    const DirectX::XMFLOAT3& positionCapsule, float radiusCapsule, float heightCapsule,
    HitResult& hitResult)
{
    using namespace DirectX;
    if (radiusSphere < 0.0f) radiusSphere = 0.0f;
    if (radiusCapsule < 0.0f) radiusCapsule = 0.0f;
    if (heightCapsule < 0.0f) heightCapsule = 0.0f;

    XMVECTOR A = XMLoadFloat3(&positionCapsule); // Capsule Base
    XMVECTOR B = XMVectorAdd(A, XMVectorSet(0.0f, heightCapsule, 0.0f, 0.0f));
    XMVECTOR C = XMLoadFloat3(&positionSphere);  // Sphere Center

    XMVECTOR AB = XMVectorSubtract(B, A);
    float ab2 = XMVectorGetX(XMVector3Dot(AB, AB));
    float t = 0.0f;
    if (ab2 > 0.0f) {
        t = XMVectorGetX(XMVector3Dot(XMVectorSubtract(C, A), AB)) / ab2;
        t = Clamp01(t);
    }
    XMVECTOR P = XMVectorAdd(A, XMVectorScale(AB, t)); // Capsule Nearest Point

    XMVECTOR D = XMVectorSubtract(C, P); // Capsule -> Sphere
    float dsq = XMVectorGetX(XMVector3Dot(D, D));
    float rr = radiusSphere + radiusCapsule;
    if (dsq > rr * rr) return false;

    float dist = dsq > 0.0f ? std::sqrt(dsq) : 0.0f;
    hitResult.hitPosition = { XMVectorGetX(P), XMVectorGetY(P), XMVectorGetZ(P) };
    hitResult.penetrationDepth = rr - dist;

    XMVECTOR n = (dist > 1e-6f) ? XMVectorScale(D, 1.0f / dist) : XMVectorSet(0, 1, 0, 0); // Capsule -> Sphere
    XMVECTOR push = XMVectorScale(n, hitResult.penetrationDepth * 0.5f);

    // Sphere(C)は n方向(離れる方向)へ加算
    // Capsule(A)は nと逆方向(離れる方向)へ減算
    XMStoreFloat3(&hitResult.selfOutPosition, XMVectorAdd(C, push));
    XMStoreFloat3(&hitResult.otherOutPosition, XMVectorSubtract(A, push));

    return true;
}

bool CollisionFunctions::IntersectCapsuleVCapsule(
    const DirectX::XMFLOAT3& positionA, float radiusA, float heightA,
    const DirectX::XMFLOAT3& positionB, float radiusB, float heightB,
    HitResult& hitResult)
{
    using namespace DirectX;
    if (radiusA < 0.0f) radiusA = 0.0f; if (radiusB < 0.0f) radiusB = 0.0f;
    if (heightA < 0.0f) heightA = 0.0f; if (heightB < 0.0f) heightB = 0.0f;

    XMVECTOR A0 = XMLoadFloat3(&positionA); // Base A
    XMVECTOR A1 = XMVectorAdd(A0, XMVectorSet(0.0f, heightA, 0.0f, 0.0f));
    XMVECTOR B0 = XMLoadFloat3(&positionB); // Base B
    XMVECTOR B1 = XMVectorAdd(B0, XMVectorSet(0.0f, heightB, 0.0f, 0.0f));

    XMVECTOR u = XMVectorSubtract(A1, A0);
    XMVECTOR v = XMVectorSubtract(B1, B0);
    XMVECTOR w = XMVectorSubtract(A0, B0);

    float a = XMVectorGetX(XMVector3Dot(u, u));
    float b = XMVectorGetX(XMVector3Dot(u, v));
    float c = XMVectorGetX(XMVector3Dot(v, v));
    float d = XMVectorGetX(XMVector3Dot(u, w));
    float e = XMVectorGetX(XMVector3Dot(v, w));

    float denom = a * c - b * b;
    float s, t;
    if (denom != 0.0f) { s = (b * e - c * d) / denom; t = (a * e - b * d) / denom; }
    else { s = (a > 0.0f) ? (-d / a) : 0.0f; t = 0.0f; }

    if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

    XMVECTOR PA = XMVectorAdd(A0, XMVectorScale(u, s));
    XMVECTOR PB = XMVectorAdd(B0, XMVectorScale(v, t));

    XMVECTOR dP = XMVectorSubtract(PA, PB); // PB -> PA (B -> A)
    float dsq = XMVectorGetX(XMVector3Dot(dP, dP));
    float rr = radiusA + radiusB;
    if (dsq > rr * rr) return false;

    float dist = dsq > 0.0f ? std::sqrt(dsq) : 0.0f;

    hitResult.hitPosition = { XMVectorGetX(PB), XMVectorGetY(PB), XMVectorGetZ(PB) };
    hitResult.penetrationDepth = rr - dist;

    // B -> A のベクトル
    XMVECTOR n = (dist > 1e-6f) ? XMVectorScale(dP, 1.0f / dist) : XMVectorSet(0, 1, 0, 0);
    XMVECTOR push = XMVectorScale(n, hitResult.penetrationDepth * 0.5f);

    // ★重要修正ポイント★
    // n は B->A の向き。
    // A を離すには n 方向へ動かす (Add)
    // B を離すには n と逆方向へ動かす (Subtract)
    XMStoreFloat3(&hitResult.selfOutPosition, XMVectorAdd(A0, push));      // A: Base + Push
    XMStoreFloat3(&hitResult.otherOutPosition, XMVectorSubtract(B0, push)); // B: Base - Push

    return true;
}

bool CollisionFunctions::IntersectSphereVsBox(
    const DirectX::XMFLOAT3& sphereCenter, float sphereRadius,
    const DirectX::XMFLOAT3& boxCenter, const DirectX::XMFLOAT3& boxSize,
    HitResult& hitResult)
{
    using namespace DirectX;

    // AABBの最小点(Min)と最大点(Max)を計算
    XMVECTOR bCenter = XMLoadFloat3(&boxCenter);
    XMVECTOR bHalf = XMLoadFloat3(&boxSize) * 0.5f;
    XMVECTOR bMin = bCenter - bHalf;
    XMVECTOR bMax = bCenter + bHalf;

    XMVECTOR sCenter = XMLoadFloat3(&sphereCenter);

    // AABB上で、球の中心に最も近い点(Closest Point)を求める
    XMVECTOR closestPoint = XMVectorMin(XMVectorMax(sCenter, bMin), bMax);

    // その点と球の中心との距離をチェック
    XMVECTOR distVec = closestPoint - sCenter;
    float distSq = XMVectorGetX(XMVector3Dot(distVec, distVec));

    // 半径の二乗より遠ければ当たっていない
    if (distSq > sphereRadius * sphereRadius) return false;

    float dist = std::sqrt(distSq);

    // 衝突情報生成
    hitResult.hitPosition = { XMVectorGetX(closestPoint), XMVectorGetY(closestPoint), XMVectorGetZ(closestPoint) };

    // 押し出しベクトル計算
    // 球の中心がボックス内にある場合(dist == 0)、特殊処理が必要だが、
    // ここでは簡易的に「最も浅い軸」などで押し出すか、ゼロ除算回避のみ行う
    XMVECTOR normal;
    float depth;

    if (dist > 1e-6f)
    {
        normal = distVec * (1.0f / dist); // Box -> Sphere (球を外へ押し出す向き)
        depth = sphereRadius - dist;
    }
    else
    {
        // 球の中心がBox内部にある場合
        // 中心から各面への距離を測り、最短の方向へ押し出すのが正解だが、
        // 簡易的にY軸上方向、あるいは前のフレームの相対位置を使うなどが一般的。
        // ここでは単純にY+へ押し出す（ハマり防止の簡易策）
        normal = XMVectorSet(0, 1, 0, 0);
        depth = sphereRadius; // 適当な深さ（本当はBox表面までの距離）

        // ※より厳密にするなら、sCenter - bMin と bMax - sCenter の各成分を見て最小の軸を選ぶ
    }

    hitResult.penetrationDepth = depth;
    XMVECTOR push = normal * (depth * 0.5f);

    // Sphereは normal方向(外)へ
    XMStoreFloat3(&hitResult.selfOutPosition, sCenter - push); // 引数のSphere側
    // Boxは normal逆方向(内=Sphereから離れる方向)へ
    XMStoreFloat3(&hitResult.otherOutPosition, bCenter + push); // 引数のBox側

    return true;
}

// ★追加実装: Box vs Box (AABB)
bool CollisionFunctions::IntersectBoxVsBox(
    const DirectX::XMFLOAT3& boxACenter, const DirectX::XMFLOAT3& boxASize,
    const DirectX::XMFLOAT3& boxBCenter, const DirectX::XMFLOAT3& boxBSize,
    HitResult& hitResult)
{
    using namespace DirectX;

    // 1. 各軸でのオーバーラップ量を計算 (SATのAABB版)
    float dx = std::abs(boxACenter.x - boxBCenter.x);
    float dy = std::abs(boxACenter.y - boxBCenter.y);
    float dz = std::abs(boxACenter.z - boxBCenter.z);

    float sumHalfX = (boxASize.x + boxBSize.x) * 0.5f;
    float sumHalfY = (boxASize.y + boxBSize.y) * 0.5f;
    float sumHalfZ = (boxASize.z + boxBSize.z) * 0.5f;

    // どこかの軸で離れていれば衝突していない
    if (dx >= sumHalfX || dy >= sumHalfY || dz >= sumHalfZ) return false;

    // 2. 衝突している場合、最も浅い貫通(Penetration)を見つける
    float penX = sumHalfX - dx;
    float penY = sumHalfY - dy;
    float penZ = sumHalfZ - dz;

    // 最小の貫通深度を持つ軸を探す
    float minPen = penX;
    int axis = 0; // 0:x, 1:y, 2:z
    if (penY < minPen) { minPen = penY; axis = 1; }
    if (penZ < minPen) { minPen = penZ; axis = 2; }

    hitResult.penetrationDepth = minPen;

    // 3. 法線を決定 (B -> A)
    // AがBに対してどの方向にいるか
    XMVECTOR n = XMVectorZero();
    if (axis == 0) n = XMVectorSet(boxACenter.x > boxBCenter.x ? 1.0f : -1.0f, 0, 0, 0);
    else if (axis == 1) n = XMVectorSet(0, boxACenter.y > boxBCenter.y ? 1.0f : -1.0f, 0, 0);
    else n = XMVectorSet(0, 0, boxACenter.z > boxBCenter.z ? 1.0f : -1.0f, 0);

    XMVECTOR push = n * (minPen * 0.5f);
    XMVECTOR posA = XMLoadFloat3(&boxACenter);
    XMVECTOR posB = XMLoadFloat3(&boxBCenter);

    // 接点は簡易的に中心の中間点とする（厳密な接点計算はAABBだともう少し複雑）
    XMVECTOR contact = (posA + posB) * 0.5f;
    XMStoreFloat3(&hitResult.hitPosition, contact);

    // 押し出し
    XMStoreFloat3(&hitResult.selfOutPosition, posA + push);      // Aは法線方向へ逃げる
    XMStoreFloat3(&hitResult.otherOutPosition, posB - push);     // Bは逆方向へ

    return true;
}

// ---------------------------------------------------------
// レイ vs 球
// ---------------------------------------------------------
bool CollisionFunctions::IntersectRayVsSphere(
    const Ray& ray,
    const DirectX::XMFLOAT3& sphereCenter,
    float sphereRadius,
    float& outT,
    DirectX::XMFLOAT3& outNormal)
{
    using namespace DirectX;


    XMVECTOR O = XMLoadFloat3(&ray.origin);
    XMVECTOR D = XMLoadFloat3(&ray.direction);
    XMVECTOR C = XMLoadFloat3(&sphereCenter);

    XMVECTOR M = O - C;
    float b = XMVectorGetX(XMVector3Dot(M, D));
    float c = XMVectorGetX(XMVector3Dot(M, M)) - (sphereRadius * sphereRadius);

    // レイの始点が球の外にあり(c > 0)、かつレイが球から離れる方向(b > 0)なら当たらない
    if (c > 0.0f && b > 0.0f) return false;

    float discr = b * b - c;
    if (discr < 0.0f) return false;

    float t = -b - sqrtf(discr);

    // t < 0 の場合、レイの始点が球の中にある => 後ろ側の交点を採用（あるいは0とする）
    if (t < 0.0f) t = 0.0f;

    outT = t;

    // 法線計算
    XMVECTOR HitPoint = O + D * t;
    XMVECTOR Normal = XMVector3Normalize(HitPoint - C);
    XMStoreFloat3(&outNormal, Normal);

    return true;
}

// ---------------------------------------------------------
// レイ vs AABB (Slab Method)
// ---------------------------------------------------------
bool CollisionFunctions::IntersectRayVsBox(
    const Ray& ray,
    const DirectX::XMFLOAT3& boxCenter,
    const DirectX::XMFLOAT3& boxSize,
    float& outT,
    DirectX::XMFLOAT3& outNormal)
{
    using namespace DirectX;


    // ボックスの最小点・最大点を計算 (Center +/- Size/2)
    XMVECTOR Center = XMLoadFloat3(&boxCenter);
    XMVECTOR Size = XMLoadFloat3(&boxSize);
    XMVECTOR HalfSize = Size * 0.5f;
    XMVECTOR Min = Center - HalfSize;
    XMVECTOR Max = Center + HalfSize;

    // レイ情報のロード
    XMVECTOR O = XMLoadFloat3(&ray.origin);
    XMVECTOR D = XMLoadFloat3(&ray.direction);

    // 各成分を float 配列で扱うための準備
    float o[3], d[3], min[3], max[3];
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(o), O);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(d), D);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(min), Min);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(max), Max);

    float tMin = 0.0f; // レイの開始地点からの最小距離
    float tMax = FLT_MAX;

    int axisSign[3] = { 0, 0, 0 }; // 法線方向の記録用

    // X, Y, Z 各軸のスラブ交差判定
    for (int i = 0; i < 3; ++i)
    {
        if (fabsf(d[i]) < EPSILON)
        {
            // レイが軸と平行な場合、スラブの外にあれば交差なし
            if (o[i] < min[i] || o[i] > max[i]) return false;
        }
        else
        {
            float invD = 1.0f / d[i];
            float t1 = (min[i] - o[i]) * invD;
            float t2 = (max[i] - o[i]) * invD;

            if (t1 > t2) std::swap(t1, t2);

            if (t1 > tMin)
            {
                tMin = t1;
                // 法線候補を記録 (i軸の負方向か正方向か)
                // t1は近平面なので、invD < 0 なら正面、invD > 0 なら負面
                axisSign[0] = (i == 0) ? (invD < 0 ? 1 : -1) : 0;
                axisSign[1] = (i == 1) ? (invD < 0 ? 1 : -1) : 0;
                axisSign[2] = (i == 2) ? (invD < 0 ? 1 : -1) : 0;
            }
            if (t2 < tMax) tMax = t2;

            if (tMin > tMax) return false;
        }
    }

    outT = tMin;
    outNormal = DirectX::XMFLOAT3((float)axisSign[0], (float)axisSign[1], (float)axisSign[2]);
    return true;
}

// ---------------------------------------------------------
// レイ vs カプセル (Infinite Cylinder + Sphere Caps)
// ---------------------------------------------------------
bool CollisionFunctions::IntersectRayVsCapsule(
    const Ray& ray,
    const DirectX::XMFLOAT3& capsuleBase,
    float radius,
    float height,
    float& outT,
    DirectX::XMFLOAT3& outNormal)
{
    using namespace DirectX;


    // カプセル構造: BaseからY軸方向にHeight伸びる円柱 + 上下の半球
    XMVECTOR Base = XMLoadFloat3(&capsuleBase);
    XMVECTOR Axis = XMVectorSet(0, 1, 0, 0); // Y軸固定
    XMVECTOR Top = Base + Axis * height;

    XMVECTOR O = XMLoadFloat3(&ray.origin);
    XMVECTOR D = XMLoadFloat3(&ray.direction);

    float closestT = FLT_MAX;
    XMVECTOR closestNormal = XMVectorZero();
    bool hit = false;

    // 1. 円柱部分との交差判定 (無限円柱として計算し、Y範囲でカット)
    XMVECTOR RC = O - Base;
    // レイ方向と円柱軸の外積 (2D平面への投影成分)
    // 無限円柱の式: |(O + tD - Base) x Axis|^2 = r^2
    // ここでは Axis=(0,1,0) なので XZ平面での距離計算に帰着

    float ox = XMVectorGetX(RC); float oz = XMVectorGetZ(RC);
    float dx = XMVectorGetX(D);  float dz = XMVectorGetZ(D);

    // 2次方程式 at^2 + bt + c = 0
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
            // float t2 = (-b + sq) / (2.0f * a); // 奥側は今回は無視

            if (t1 > 0.0f)
            {
                // 高さ範囲チェック
                float y = XMVectorGetY(O) + XMVectorGetY(D) * t1;
                float baseY = XMVectorGetY(Base);

                if (y >= baseY && y <= baseY + height)
                {
                    closestT = t1;
                    hit = true;
                    // 法線: 衝突点から軸へのベクトル (XZ成分のみ正規化)
                    XMVECTOR HitP = O + D * t1;
                    XMVECTOR N = HitP - XMVectorSet(XMVectorGetX(HitP), y, XMVectorGetZ(HitP), 0);
                    // 上記は (x, 0, z) になるはずだが、中心軸が (base.x, y, base.z) なので
                    // Baseが原点でない場合を考慮
                    XMVECTOR AxisPt = Base + Axis * (y - baseY);
                    closestNormal = XMVector3Normalize(HitP - AxisPt);
                }
            }
        }
    }

    // 2. 下の球 (Base, radius) との交差
    float tSphere;
    DirectX::XMFLOAT3 nSphere;
    if (IntersectRayVsSphere(ray, capsuleBase, radius, tSphere, nSphere))
    {
        // 球の上半分（円柱内部）は無視したいが、今回はシンプルに距離で勝負
        if (tSphere < closestT)
        {
            closestT = tSphere;
            closestNormal = XMLoadFloat3(&nSphere);
            hit = true;
        }
    }

    // 3. 上の球 (Top, radius) との交差
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

    if (hit)
    {
        outT = closestT;
        XMStoreFloat3(&outNormal, closestNormal);
        return true;
    }

    return false;
}
