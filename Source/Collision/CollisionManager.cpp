// CollisionManager.cpp
#include "CollisionManager.h"
#include "CollisionFunctions.h"
#include <algorithm>

// singleton インスタンスを返す。
CollisionManager& CollisionManager::Instance()
{
    static CollisionManager instance;
    return instance;
}

// 球コライダーを登録する。
// 登録成功時は一意 ID を返す。
uint32_t CollisionManager::AddSphere(const SphereDesc& desc, void* userPtr, ColliderAttribute attr)
{
    Collider c;
    c.id = issueId_++;
    c.shape = ColliderShape::Sphere;
    c.userPtr = userPtr;
    c.attribute = attr;
    c.enabled = true;
    c.sphere = desc;

    // 半径が負値にならないよう補正する。
    if (c.sphere.radius < 0.0f) c.sphere.radius = 0.0f;

    colliders_.push_back(c);
    return c.id;
}

// カプセルコライダーを登録する。
// 登録成功時は一意 ID を返す。
uint32_t CollisionManager::AddCapsule(const CapsuleDesc& desc, void* userPtr, ColliderAttribute attr)
{
    Collider c;
    c.id = issueId_++;
    c.shape = ColliderShape::Capsule;
    c.userPtr = userPtr;
    c.attribute = attr;
    c.enabled = true;
    c.capsule = desc;

    // 半径と高さが負値にならないよう補正する。
    if (c.capsule.radius < 0.0f) c.capsule.radius = 0.0f;
    if (c.capsule.height < 0.0f) c.capsule.height = 0.0f;

    colliders_.push_back(c);
    return c.id;
}

// Box コライダーを登録する。
// 登録成功時は一意 ID を返す。
uint32_t CollisionManager::AddBox(const BoxDesc& desc, void* userPtr, ColliderAttribute attr)
{
    Collider c;
    c.id = issueId_++;
    c.shape = ColliderShape::Box;
    c.userPtr = userPtr;
    c.attribute = attr;
    c.enabled = true;
    c.box = desc;

    // サイズは常に正値として扱う。
    c.box.size.x = std::abs(c.box.size.x);
    c.box.size.y = std::abs(c.box.size.y);
    c.box.size.z = std::abs(c.box.size.z);

    colliders_.push_back(c);
    return c.id;
}

// 指定 ID の球コライダー形状を更新する。
bool CollisionManager::UpdateSphere(uint32_t id, const SphereDesc& desc)
{
    for (auto& c : colliders_) {
        if (c.id == id && c.shape == ColliderShape::Sphere) {
            c.sphere = desc;

            // 半径が負値にならないよう補正する。
            if (c.sphere.radius < 0.0f) c.sphere.radius = 0.0f;
            return true;
        }
    }
    return false;
}

// 指定 ID のカプセルコライダー形状を更新する。
bool CollisionManager::UpdateCapsule(uint32_t id, const CapsuleDesc& desc)
{
    for (auto& c : colliders_) {
        if (c.id == id && c.shape == ColliderShape::Capsule) {
            c.capsule = desc;

            // 半径と高さが負値にならないよう補正する。
            if (c.capsule.radius < 0.0f) c.capsule.radius = 0.0f;
            if (c.capsule.height < 0.0f) c.capsule.height = 0.0f;
            return true;
        }
    }
    return false;
}

// 指定 ID の Box コライダー形状を更新する。
bool CollisionManager::UpdateBox(uint32_t id, const BoxDesc& desc)
{
    for (auto& c : colliders_) {
        if (c.id == id && c.shape == ColliderShape::Box) {
            c.box = desc;

            // サイズは常に正値として扱う。
            c.box.size.x = std::abs(c.box.size.x);
            c.box.size.y = std::abs(c.box.size.y);
            c.box.size.z = std::abs(c.box.size.z);
            return true;
        }
    }
    return false;
}

// 指定 ID の enabled 状態を変更する。
bool CollisionManager::SetEnabled(uint32_t id, bool enabled)
{
    for (auto& c : colliders_) {
        if (c.id == id) {
            c.enabled = enabled;
            return true;
        }
    }
    return false;
}

// 指定 ID の enabled 状態を取得する。
// 見つからなければ false を返す。
bool CollisionManager::GetEnabled(uint32_t id) const
{
    for (auto& c : colliders_) {
        if (c.id == id) return c.enabled;
    }
    return false;
}

// 指定 ID の userPtr を更新する。
bool CollisionManager::SetUserPtr(uint32_t id, void* p)
{
    for (auto& c : colliders_) {
        if (c.id == id) {
            c.userPtr = p;
            return true;
        }
    }
    return false;
}

// 指定 ID の userPtr を取得する。
// 見つからなければ nullptr を返す。
void* CollisionManager::GetUserPtr(uint32_t id) const
{
    for (auto& c : colliders_) {
        if (c.id == id) return c.userPtr;
    }
    return nullptr;
}

// 指定 ID のコライダー本体を取得する。
// 見つからなければ nullptr を返す。
const Collider* CollisionManager::Get(uint32_t id) const
{
    for (auto& c : colliders_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

// 指定 ID のコライダーを削除する。
bool CollisionManager::Remove(uint32_t id)
{
    auto it = std::find_if(colliders_.begin(), colliders_.end(),
        [&](const Collider& c) { return c.id == id; });

    if (it == colliders_.end()) return false;

    colliders_.erase(it);
    return true;
}

// すべてのコライダーを削除し、ID 発行カウンタも初期化する。
void CollisionManager::Clear()
{
    colliders_.clear();
    issueId_ = 1;
}

// 登録中の全コライダー組み合わせについて接触判定を行い、
// 接触しているペアを outContacts に書き出す。
void CollisionManager::ComputeAllContacts(std::vector<CollisionContact>& outContacts) const
{
    outContacts.clear();

    const size_t n = colliders_.size();
    for (size_t i = 0; i < n; ++i) {
        const Collider& A = colliders_[i];
        if (!A.enabled) continue;

        for (size_t j = i + 1; j < n; ++j) {
            const Collider& B = colliders_[j];
            if (!B.enabled) continue;

            HitResult hit{};
            bool ok = false;

            // 形状の組み合わせごとに対応する判定関数を呼ぶ。
            if (A.shape == ColliderShape::Sphere && B.shape == ColliderShape::Sphere) {
                ok = CollisionFunctions::IntersectSphereVsSphere(
                    A.sphere.center, A.sphere.radius,
                    B.sphere.center, B.sphere.radius,
                    hit);
            }
            else if (A.shape == ColliderShape::Sphere && B.shape == ColliderShape::Capsule) {
                ok = CollisionFunctions::IntersectSphereVsCapsule(
                    A.sphere.center, A.sphere.radius,
                    B.capsule.base, B.capsule.radius, B.capsule.height,
                    hit);
            }
            else if (A.shape == ColliderShape::Capsule && B.shape == ColliderShape::Sphere) {
                ok = CollisionFunctions::IntersectSphereVsCapsule(
                    B.sphere.center, B.sphere.radius,
                    A.capsule.base, A.capsule.radius, A.capsule.height,
                    hit);

                // 判定関数は Sphere を self 扱いするので、順番を入れ替えたぶん結果も反転する。
                if (ok) std::swap(hit.selfOutPosition, hit.otherOutPosition);
            }
            else if (A.shape == ColliderShape::Capsule && B.shape == ColliderShape::Capsule) {
                ok = CollisionFunctions::IntersectCapsuleVCapsule(
                    A.capsule.base, A.capsule.radius, A.capsule.height,
                    B.capsule.base, B.capsule.radius, B.capsule.height,
                    hit);
            }
            else if (A.shape == ColliderShape::Sphere && B.shape == ColliderShape::Box) {
                ok = CollisionFunctions::IntersectSphereVsBox(
                    A.sphere.center, A.sphere.radius,
                    B.box.center, B.box.size,
                    hit);
            }
            else if (A.shape == ColliderShape::Box && B.shape == ColliderShape::Sphere) {
                ok = CollisionFunctions::IntersectSphereVsBox(
                    B.sphere.center, B.sphere.radius,
                    A.box.center, A.box.size,
                    hit);

                // 判定順を入れ替えたので押し戻し結果も反転する。
                if (ok) std::swap(hit.selfOutPosition, hit.otherOutPosition);
            }
            else if (A.shape == ColliderShape::Box && B.shape == ColliderShape::Box) {
                ok = CollisionFunctions::IntersectBoxVsBox(
                    A.box.center, A.box.size,
                    B.box.center, B.box.size,
                    hit);
            }

            // 接触していれば contact として追加する。
            if (ok) {
                CollisionContact c;
                c.idA = (uint32_t)A.id;
                c.idB = (uint32_t)B.id;
                c.hit = hit;
                outContacts.push_back(c);
            }
        }
    }
}

// レイを全コライダーへ飛ばし、最も近いヒット 1 件を返す。
// maxDistance 以内にヒットが無ければ false。
bool CollisionManager::Raycast(const Ray& ray, RaycastHit& outHit, float maxDistance)
{
    bool hasHit = false;
    float closestDist = maxDistance;

    for (const auto& c : colliders_)
    {
        // 無効コライダーは無視する。
        if (!c.enabled) continue;

        float t = FLT_MAX;
        DirectX::XMFLOAT3 normal;
        bool hit = false;

        // 形状ごとに対応するレイ判定関数を呼ぶ。
        switch (c.shape)
        {
        case ColliderShape::Sphere:
            hit = CollisionFunctions::IntersectRayVsSphere(
                ray, c.sphere.center, c.sphere.radius, t, normal);
            break;

        case ColliderShape::Capsule:
            hit = CollisionFunctions::IntersectRayVsCapsule(
                ray, c.capsule.base, c.capsule.radius, c.capsule.height, t, normal);
            break;

        case ColliderShape::Box:
            hit = CollisionFunctions::IntersectRayVsBox(
                ray, c.box.center, c.box.size, t, normal);
            break;
        }

        // より近いヒットだけを採用する。
        if (hit && t >= 0.0f && t < closestDist)
        {
            closestDist = t;
            hasHit = true;

            outHit.distance = t;
            outHit.normal = normal;
            outHit.userPtr = c.userPtr;

            // ヒット位置を計算しているが、現状 outHit へは保存していない。
            DirectX::XMVECTOR Dir = DirectX::XMLoadFloat3(&ray.direction);
            DirectX::XMVECTOR HitPoint = DirectX::XMVectorAdd(
                DirectX::XMLoadFloat3(&ray.origin),
                DirectX::XMVectorScale(Dir, t)
            );

            // 必要ならここで outHit.hitPosition などへ保存する拡張余地がある。
        }
    }

    return hasHit;
}