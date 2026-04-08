#include "Transform/NodeAttachmentSystem.h"

#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NodeAttachmentComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Transform/NodeAttachmentUtils.h"

using namespace DirectX;

namespace
{
    XMMATRIX BuildLocalMatrix(const TransformComponent& transform)
    {
        return XMMatrixAffineTransformation(
            XMLoadFloat3(&transform.localScale),
            XMVectorZero(),
            XMLoadFloat4(&transform.localRotation),
            XMLoadFloat3(&transform.localPosition));
    }

    void WriteTransformFromMatrices(TransformComponent& transform, const XMFLOAT4X4& localMatrix, const XMFLOAT4X4& worldMatrix)
    {
        transform.prevWorldMatrix = transform.worldMatrix;
        transform.localMatrix = localMatrix;
        transform.worldMatrix = worldMatrix;

        XMVECTOR localScale;
        XMVECTOR localRot;
        XMVECTOR localPos;
        if (XMMatrixDecompose(&localScale, &localRot, &localPos, XMLoadFloat4x4(&localMatrix))) {
            XMStoreFloat3(&transform.localScale, localScale);
            XMStoreFloat4(&transform.localRotation, localRot);
            XMStoreFloat3(&transform.localPosition, localPos);
        }

        XMVECTOR worldScale;
        XMVECTOR worldRot;
        XMVECTOR worldPos;
        if (XMMatrixDecompose(&worldScale, &worldRot, &worldPos, XMLoadFloat4x4(&worldMatrix))) {
            XMStoreFloat3(&transform.worldScale, worldScale);
            XMStoreFloat4(&transform.worldRotation, worldRot);
            XMStoreFloat3(&transform.worldPosition, worldPos);
        }

        transform.isDirty = false;
    }

    void ApplyWorldRecursive(EntityID entity, const XMFLOAT4X4& desiredWorld, Registry& registry)
    {
        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform) {
            return;
        }

        XMMATRIX parentWorld = XMMatrixIdentity();
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            if (!Entity::IsNull(hierarchy->parent)) {
                if (auto* parentTransform = registry.GetComponent<TransformComponent>(hierarchy->parent)) {
                    parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
                }
            }
        }

        const XMMATRIX world = XMLoadFloat4x4(&desiredWorld);
        XMFLOAT4X4 localMatrix;
        XMStoreFloat4x4(&localMatrix, world * XMMatrixInverse(nullptr, parentWorld));
        WriteTransformFromMatrices(*transform, localMatrix, desiredWorld);

        auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy) {
            return;
        }

        EntityID child = hierarchy->firstChild;
        while (!Entity::IsNull(child)) {
            if (auto* childTransform = registry.GetComponent<TransformComponent>(child)) {
                XMFLOAT4X4 childWorld;
                XMStoreFloat4x4(&childWorld, BuildLocalMatrix(*childTransform) * world);
                ApplyWorldRecursive(child, childWorld, registry);
            }

            auto* childHierarchy = registry.GetComponent<HierarchyComponent>(child);
            child = childHierarchy ? childHierarchy->nextSibling : Entity::NULL_ID;
        }
    }
}

void NodeAttachmentSystem::Update(Registry& registry)
{
    Query<NodeAttachmentComponent, TransformComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, NodeAttachmentComponent& attachment, TransformComponent&) {
        if (!attachment.enabled || !attachment.attached) {
            return;
        }

        if (Entity::IsNull(attachment.targetEntity) || !registry.IsAlive(attachment.targetEntity)) {
            return;
        }

        auto* targetTransform = registry.GetComponent<TransformComponent>(attachment.targetEntity);
        auto* targetMesh = registry.GetComponent<MeshComponent>(attachment.targetEntity);
        if (!targetTransform || !targetMesh || !targetMesh->model) {
            return;
        }

        NodeSocketComponent* sockets = registry.GetComponent<NodeSocketComponent>(attachment.targetEntity);
        XMFLOAT4X4 desiredWorld;
        if (!NodeAttachmentUtils::TryResolveNamedAttachmentWorldMatrix(
                targetMesh->model.get(),
                targetTransform->worldMatrix,
                sockets,
                attachment.attachName,
                attachment.useSocket,
                attachment.cachedBoneIndex,
                attachment.offsetLocal,
                attachment.offsetRotDeg,
                attachment.offsetScale,
                attachment.offsetSpace,
                desiredWorld)) {
            return;
        }

        ApplyWorldRecursive(entity, desiredWorld, registry);
    });
}
