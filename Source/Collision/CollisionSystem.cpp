#include "CollisionSystem.h"

#include "Collision/CollisionManager.h"
#include "Transform/NodeAttachmentUtils.h"
#include <System/Query.h>
#include <cmath>

using namespace DirectX;

namespace
{
    float ResolveAxisScale(float value, float fallback)
    {
        const float absValue = std::fabs(value);
        if (absValue > 0.0001f) {
            return absValue;
        }
        return fallback > 0.0001f ? fallback : 1.0f;
    }

    float ResolveMaxWorldScale(const TransformComponent& transform)
    {
        float value = std::fabs(transform.worldScale.x);

        const float sy = std::fabs(transform.worldScale.y);
        if (sy > value) {
            value = sy;
        }

        const float sz = std::fabs(transform.worldScale.z);
        if (sz > value) {
            value = sz;
        }

        if (value <= 0.0001f) {
            value = 1.0f;
        }
        return value;
    }

    XMFLOAT3 ResolveColliderWorldPosition(
        Registry& registry,
        EntityID entity,
        const ColliderComponent::Element& element,
        const TransformComponent& transform)
    {
        const XMMATRIX worldMatrix = XMLoadFloat4x4(&transform.worldMatrix);
        const XMFLOAT3 localOffset = {
            element.offsetLocal.x,
            element.offsetLocal.y,
            element.offsetLocal.z
        };

        XMVECTOR worldPosition = XMVectorZero();
        if (element.nodeIndex >= 0) {
            MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
            if (mesh && mesh->model) {
                const XMFLOAT3 nodeLocalPosition =
                    NodeAttachmentUtils::GetWorldPositionNodeLocal(
                        mesh->model.get(),
                        element.nodeIndex,
                        localOffset);
                worldPosition = XMVector3TransformCoord(XMLoadFloat3(&nodeLocalPosition), worldMatrix);
            }
            else {
                worldPosition = XMVector3TransformCoord(XMLoadFloat3(&localOffset), worldMatrix);
            }
        }
        else {
            worldPosition = XMVector3TransformCoord(XMLoadFloat3(&localOffset), worldMatrix);
        }

        XMFLOAT3 result{};
        XMStoreFloat3(&result, worldPosition);
        return result;
    }

    void RefreshSphereCollider(
        CollisionManager& collisionManager,
        EntityID entity,
        ColliderComponent::Element& element,
        const SphereDesc& desc)
    {
        if (element.registeredId != 0) {
            if (!collisionManager.UpdateSphere(element.registeredId, desc)) {
                collisionManager.Remove(element.registeredId);
                element.registeredId = 0;
            }
        }

        if (element.registeredId == 0) {
            element.registeredId = collisionManager.AddSphere(
                desc,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                element.attribute);
        }

        if (element.registeredId != 0) {
            collisionManager.SetEnabled(element.registeredId, true);
            collisionManager.SetUserPtr(
                element.registeredId,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
        }
    }

    void RefreshCapsuleCollider(
        CollisionManager& collisionManager,
        EntityID entity,
        ColliderComponent::Element& element,
        const CapsuleDesc& desc)
    {
        if (element.registeredId != 0) {
            if (!collisionManager.UpdateCapsule(element.registeredId, desc)) {
                collisionManager.Remove(element.registeredId);
                element.registeredId = 0;
            }
        }

        if (element.registeredId == 0) {
            element.registeredId = collisionManager.AddCapsule(
                desc,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                element.attribute);
        }

        if (element.registeredId != 0) {
            collisionManager.SetEnabled(element.registeredId, true);
            collisionManager.SetUserPtr(
                element.registeredId,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
        }
    }

    void RefreshBoxCollider(
        CollisionManager& collisionManager,
        EntityID entity,
        ColliderComponent::Element& element,
        const BoxDesc& desc)
    {
        if (element.registeredId != 0) {
            if (!collisionManager.UpdateBox(element.registeredId, desc)) {
                collisionManager.Remove(element.registeredId);
                element.registeredId = 0;
            }
        }

        if (element.registeredId == 0) {
            element.registeredId = collisionManager.AddBox(
                desc,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                element.attribute);
        }

        if (element.registeredId != 0) {
            collisionManager.SetEnabled(element.registeredId, true);
            collisionManager.SetUserPtr(
                element.registeredId,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
        }
    }
}

void CollisionSystem::Update(Registry& registry)
{
    auto& collisionManager = CollisionManager::Instance();

    Query<ColliderComponent, TransformComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, ColliderComponent& collider, TransformComponent& transform) {
        if (!collider.enabled) {
            for (auto& element : collider.elements) {
                if (element.runtimeTag != 0) {
                    continue;
                }

                if (element.registeredId != 0) {
                    collisionManager.SetEnabled(element.registeredId, false);
                }
            }
            return;
        }

        const float maxWorldScale = ResolveMaxWorldScale(transform);
        const float scaleX = ResolveAxisScale(transform.worldScale.x, maxWorldScale);
        const float scaleY = ResolveAxisScale(transform.worldScale.y, maxWorldScale);
        const float scaleZ = ResolveAxisScale(transform.worldScale.z, maxWorldScale);

        for (auto& element : collider.elements) {
            if (element.runtimeTag != 0) {
                continue;
            }

            if (!element.enabled) {
                if (element.registeredId != 0) {
                    collisionManager.SetEnabled(element.registeredId, false);
                }
                continue;
            }

            const XMFLOAT3 worldPosition =
                ResolveColliderWorldPosition(registry, entity, element, transform);

            switch (element.type) {
            case ColliderShape::Sphere:
                RefreshSphereCollider(
                    collisionManager,
                    entity,
                    element,
                    { worldPosition, element.radius * maxWorldScale });
                break;

            case ColliderShape::Capsule:
                RefreshCapsuleCollider(
                    collisionManager,
                    entity,
                    element,
                    { worldPosition, element.radius * maxWorldScale, element.height * scaleY });
                break;

            case ColliderShape::Box:
                RefreshBoxCollider(
                    collisionManager,
                    entity,
                    element,
                    {
                        worldPosition,
                        {
                            element.size.x * scaleX,
                            element.size.y * scaleY,
                            element.size.z * scaleZ
                        }
                    });
                break;
            }
        }
    });
}

void CollisionSystem::Finalize(Registry& registry)
{
    Query<ColliderComponent> query(registry);
    query.ForEach([](ColliderComponent& collider) {
        for (auto& element : collider.elements) {
            if (element.registeredId != 0) {
                CollisionManager::Instance().Remove(element.registeredId);
                element.registeredId = 0;
            }
        }
    });
}
