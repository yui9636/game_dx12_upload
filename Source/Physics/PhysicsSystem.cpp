#include "PhysicsSystem.h"
#include "Component/HierarchyComponent.h"
#include <System\Query.h>

using namespace DirectX;

namespace
{
    EntityID GetTransformParent(Registry& registry, EntityID entity, const TransformComponent& trans)
    {
        if (HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }
        return trans.parent == 0 ? Entity::NULL_ID : trans.parent;
    }

    void StoreWorldBodyToTransform(
        Registry& registry,
        EntityID entity,
        TransformComponent& trans,
        const JPH::RVec3& pos,
        const JPH::Quat& rot)
    {
        const XMVECTOR worldPosition = XMVectorSet(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ()),
            1.0f);
        const XMVECTOR worldRotation = XMVectorSet(
            rot.GetX(),
            rot.GetY(),
            rot.GetZ(),
            rot.GetW());

        XMStoreFloat3(&trans.worldPosition, worldPosition);
        XMStoreFloat4(&trans.worldRotation, worldRotation);

        const EntityID parent = GetTransformParent(registry, entity, trans);
        TransformComponent* parentTransform = !Entity::IsNull(parent)
            ? registry.GetComponent<TransformComponent>(parent)
            : nullptr;

        if (parentTransform) {
            const XMMATRIX worldMatrix = XMMatrixAffineTransformation(
                XMLoadFloat3(&trans.worldScale),
                XMVectorZero(),
                worldRotation,
                worldPosition);
            const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
            const XMMATRIX localMatrix = worldMatrix * XMMatrixInverse(nullptr, parentWorld);

            XMVECTOR localScale;
            XMVECTOR localRotation;
            XMVECTOR localPosition;
            if (XMMatrixDecompose(&localScale, &localRotation, &localPosition, localMatrix)) {
                XMStoreFloat3(&trans.localPosition, localPosition);
                XMStoreFloat4(&trans.localRotation, localRotation);
            }
        }
        else {
            XMStoreFloat3(&trans.localPosition, worldPosition);
            XMStoreFloat4(&trans.localRotation, worldRotation);
        }

        trans.isDirty = true;
    }
}

// Steps Jolt and writes body transforms back to ECS local transforms.
void PhysicsSystem::Update(Registry& registry, float deltaTime) {
    auto& physicsMgr = PhysicsManager::Instance();

    physicsMgr.Update(deltaTime);

    JPH::BodyInterface& bodyInterface = physicsMgr.GetBodyInterface();

    Query<PhysicsComponent, TransformComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, const PhysicsComponent& phys, TransformComponent& trans) {
        if (phys.bodyID.IsInvalid()) {
            return;
        }

        JPH::RVec3 joltPos = bodyInterface.GetPosition(phys.bodyID);
        JPH::Quat joltRot = bodyInterface.GetRotation(phys.bodyID);

        StoreWorldBodyToTransform(registry, entity, trans, joltPos, joltRot);
    });
}
