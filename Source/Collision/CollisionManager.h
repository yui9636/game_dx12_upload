// CollisionManager.h
#pragma once
#include <vector>
#include <cstdint>
#include "Collision.h"

struct CollisionContact
{
    uint32_t  idA{ 0 };
    uint32_t  idB{ 0 };
    HitResult hit{};
};

class CollisionManager
{
public:
    static CollisionManager& Instance();

    uint32_t AddSphere(const SphereDesc& desc, void* userPtr = nullptr, ColliderAttribute attr = ColliderAttribute::Body);
    uint32_t AddCapsule(const CapsuleDesc& desc, void* userPtr = nullptr, ColliderAttribute attr = ColliderAttribute::Body);

    uint32_t AddBox(const BoxDesc& desc, void* userPtr = nullptr, ColliderAttribute attr = ColliderAttribute::Body);

    bool UpdateBox(uint32_t id, const BoxDesc& desc);
    bool UpdateSphere(uint32_t id, const SphereDesc& desc);
    bool UpdateCapsule(uint32_t id, const CapsuleDesc& desc);

    bool SetEnabled(uint32_t id, bool enabled);
    bool GetEnabled(uint32_t id) const;

    bool   SetUserPtr(uint32_t id, void* p);
    void* GetUserPtr(uint32_t id) const;

    const Collider* Get(uint32_t id) const;
    bool Remove(uint32_t id);
    void Clear();

    void ComputeAllContacts(std::vector<CollisionContact>& outContacts) const;

    const std::vector<Collider>& GetAll() const { return colliders_; }

    bool Raycast(const ::Ray& ray, RaycastHit& outHit, float maxDistance = FLT_MAX);
private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    void operator=(const CollisionManager&) = delete;

    uint32_t issueId_ = 1;
    std::vector<Collider> colliders_;
};
