#include "PhysicsSyncSystem.h"
#include "Component/HierarchyComponent.h"
#include "Component/TransformComponent.h"
#include "Component/PhysicsComponent.h"
#include "Physics/PhysicsManager.h"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <System\Query.h>

using namespace JPH;
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
        const RVec3& pos,
        const Quat& rot)
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

// Jolt body と ECS transform を同期する。
void PhysicsSyncSystem::Update(Registry& registry, bool isSimulation) {
    auto& physicsMgr = PhysicsManager::Instance();
    BodyInterface& bodyInterface = physicsMgr.GetBodyInterface();

    Query<TransformComponent, PhysicsComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans, PhysicsComponent& phys) {
        if (phys.bodyID.IsInvalid()) return;

        if (!isSimulation) {
            if (trans.isDirty) {
                RVec3 newPos(trans.worldPosition.x, trans.worldPosition.y, trans.worldPosition.z);
                Quat newRot(trans.worldRotation.x, trans.worldRotation.y, trans.worldRotation.z, trans.worldRotation.w);

                bodyInterface.SetPositionAndRotation(phys.bodyID, newPos, newRot, EActivation::DontActivate);
            }
        }
        else {
            if (bodyInterface.GetMotionType(phys.bodyID) != EMotionType::Static) {
                RVec3 pos;
                Quat rot;
                bodyInterface.GetPositionAndRotation(phys.bodyID, pos, rot);

                StoreWorldBodyToTransform(registry, entity, trans, pos, rot);

            }
        }
        });
}
