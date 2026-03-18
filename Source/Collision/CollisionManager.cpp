// CollisionManager.cpp
#include "CollisionManager.h"
#include "CollisionFunctions.h"
#include <algorithm>

// ★追加: シングルトンインスタンスの実装
CollisionManager& CollisionManager::Instance()
{
    static CollisionManager instance;
    return instance;
}

uint32_t CollisionManager::AddSphere(const SphereDesc& desc, void* userPtr, ColliderAttribute attr)
{
    Collider c;
    c.id = issueId_++;
    c.shape = ColliderShape::Sphere;
    c.userPtr = userPtr;
    c.attribute = attr; // ★セット
    c.enabled = true;
    c.sphere = desc;
    if (c.sphere.radius < 0.0f) c.sphere.radius = 0.0f;
    colliders_.push_back(c);
    return c.id;
}

// ★変更: attr 引数を受け取り、c.attribute にセットする
uint32_t CollisionManager::AddCapsule(const CapsuleDesc& desc, void* userPtr, ColliderAttribute attr)
{
    Collider c;
    c.id = issueId_++;
    c.shape = ColliderShape::Capsule;
    c.userPtr = userPtr;
    c.attribute = attr; // ★セット
    c.enabled = true;
    c.capsule = desc;
    if (c.capsule.radius < 0.0f) c.capsule.radius = 0.0f;
    if (c.capsule.height < 0.0f) c.capsule.height = 0.0f;
    colliders_.push_back(c);
    return c.id;
}

uint32_t CollisionManager::AddBox(const BoxDesc& desc, void* userPtr, ColliderAttribute attr)
{
    Collider c;
    c.id = issueId_++;
    c.shape = ColliderShape::Box;
    c.userPtr = userPtr;
    c.attribute = attr;
    c.enabled = true;
    c.box = desc;
    // 負のサイズ防止
    c.box.size.x = std::abs(c.box.size.x);
    c.box.size.y = std::abs(c.box.size.y);
    c.box.size.z = std::abs(c.box.size.z);
    colliders_.push_back(c);
    return c.id;
}

bool CollisionManager::UpdateSphere(uint32_t id, const SphereDesc& desc)
{
    for (auto& c : colliders_) if (c.id == id && c.shape == ColliderShape::Sphere) { c.sphere = desc; if (c.sphere.radius < 0.0f) c.sphere.radius = 0.0f; return true; }
    return false;
}
bool CollisionManager::UpdateCapsule(uint32_t id, const CapsuleDesc& desc)
{
    for (auto& c : colliders_) if (c.id == id && c.shape == ColliderShape::Capsule) { c.capsule = desc; if (c.capsule.radius < 0.0f) c.capsule.radius = 0.0f; if (c.capsule.height < 0.0f) c.capsule.height = 0.0f; return true; }
    return false;
}
bool CollisionManager::UpdateBox(uint32_t id, const BoxDesc& desc)
{
    for (auto& c : colliders_) if (c.id == id && c.shape == ColliderShape::Box) {
        c.box = desc;
        c.box.size.x = std::abs(c.box.size.x);
        c.box.size.y = std::abs(c.box.size.y);
        c.box.size.z = std::abs(c.box.size.z);
        return true;
    }
    return false;
}

bool CollisionManager::SetEnabled(uint32_t id, bool enabled)
{
    for (auto& c : colliders_) if (c.id == id) { c.enabled = enabled; return true; }
    return false;
}
bool CollisionManager::GetEnabled(uint32_t id) const
{
    for (auto& c : colliders_) if (c.id == id) return c.enabled;
    return false;
}

bool CollisionManager::SetUserPtr(uint32_t id, void* p)
{
    for (auto& c : colliders_) if (c.id == id) { c.userPtr = p; return true; }
    return false;
}
void* CollisionManager::GetUserPtr(uint32_t id) const
{
    for (auto& c : colliders_) if (c.id == id) return c.userPtr;
    return nullptr;
}

const Collider* CollisionManager::Get(uint32_t id) const
{
    for (auto& c : colliders_) if (c.id == id) return &c;
    return nullptr;
}

bool CollisionManager::Remove(uint32_t id)
{
    auto it = std::find_if(colliders_.begin(), colliders_.end(), [&](const Collider& c) { return c.id == id; });
    if (it == colliders_.end()) return false;
    colliders_.erase(it);
    return true;
}

void CollisionManager::Clear()
{
    colliders_.clear(); issueId_ = 1;
}

void CollisionManager::ComputeAllContacts(std::vector<CollisionContact>& outContacts) const
{
    outContacts.clear();
    const size_t n = colliders_.size();
    for (size_t i = 0; i < n; ++i) {
        const Collider& A = colliders_[i]; if (!A.enabled) continue;
        for (size_t j = i + 1; j < n; ++j) {
            const Collider& B = colliders_[j]; if (!B.enabled) continue;

            HitResult hit{}; bool ok = false;
            if (A.shape == ColliderShape::Sphere && B.shape == ColliderShape::Sphere) {
                ok = CollisionFunctions::IntersectSphereVsSphere(A.sphere.center, A.sphere.radius, B.sphere.center, B.sphere.radius, hit);
            }
            else if (A.shape == ColliderShape::Sphere && B.shape == ColliderShape::Capsule) {
                ok = CollisionFunctions::IntersectSphereVsCapsule(A.sphere.center, A.sphere.radius, B.capsule.base, B.capsule.radius, B.capsule.height, hit);
            }
            else if (A.shape == ColliderShape::Capsule && B.shape == ColliderShape::Sphere) {
                ok = CollisionFunctions::IntersectSphereVsCapsule(B.sphere.center, B.sphere.radius, A.capsule.base, A.capsule.radius, A.capsule.height, hit);
                if (ok) std::swap(hit.selfOutPosition, hit.otherOutPosition);
            }
            else if (A.shape == ColliderShape::Capsule && B.shape == ColliderShape::Capsule) {
                ok = CollisionFunctions::IntersectCapsuleVCapsule(A.capsule.base, A.capsule.radius, A.capsule.height, B.capsule.base, B.capsule.radius, B.capsule.height, hit);
            }
            // 5. Sphere vs Box (★追加)
            else if (A.shape == ColliderShape::Sphere && B.shape == ColliderShape::Box) {
                ok = CollisionFunctions::IntersectSphereVsBox(A.sphere.center, A.sphere.radius, B.box.center, B.box.size, hit);
            }
            // 6. Box vs Sphere (★追加)
            else if (A.shape == ColliderShape::Box && B.shape == ColliderShape::Sphere) {
                ok = CollisionFunctions::IntersectSphereVsBox(B.sphere.center, B.sphere.radius, A.box.center, A.box.size, hit);
                if (ok) std::swap(hit.selfOutPosition, hit.otherOutPosition);
            }
            // 7. Box vs Box (★追加)
            else if (A.shape == ColliderShape::Box && B.shape == ColliderShape::Box) {
                ok = CollisionFunctions::IntersectBoxVsBox(A.box.center, A.box.size, B.box.center, B.box.size, hit);
            }


            if (ok) {
                CollisionContact c; c.idA = (uint32_t)A.id; c.idB = (uint32_t)B.id; c.hit = hit;
                outContacts.push_back(c);
            }
        }
    }
}

bool CollisionManager::Raycast(const Ray& ray, RaycastHit& outHit, float maxDistance)
{
    bool hasHit = false;
    float closestDist = maxDistance;

    // 登録されている全コライダーを走査
    // colliders_ は std::vector<Collider> と想定
    for (const auto& c : colliders_)
    {
        // 無効なものはスキップ
        if (!c.enabled) continue;

        // ヒット情報の一時変数
        float t = FLT_MAX;
        DirectX::XMFLOAT3 normal;
        bool hit = false;

        // 形状ごとの判定
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

        // ヒットしており、かつ既存のヒットより手前なら更新
        if (hit && t >= 0.0f && t < closestDist)
        {
            closestDist = t;
            hasHit = true;

            // 結果を格納
            outHit.distance = t;
            outHit.normal = normal;
            outHit.userPtr = c.userPtr; // これがアクター(Actor*)になる

            // 衝突座標の計算: Origin + Dir * t
            DirectX::XMVECTOR Dir = DirectX::XMLoadFloat3(&ray.direction);
            DirectX::XMVECTOR HitPoint = DirectX::XMVectorAdd(
                DirectX::XMLoadFloat3(&ray.origin),
                DirectX::XMVectorScale(Dir, t) // ここで t を掛ける
            );


        }
    }
        return hasHit;

    
}


