#include "TransformSystem.h"
#include <System\Query.h>
#include <unordered_set>

using namespace DirectX;

namespace
{
    EntityID GetParentEntity(Registry& registry, EntityID entity, const TransformComponent& transform)
    {
        if (HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }

        return transform.parent == 0 ? Entity::NULL_ID : transform.parent;
    }

    void ComputeEntityWorld(
        Registry& registry,
        EntityID entity,
        std::unordered_set<EntityID>& computed,
        std::unordered_set<EntityID>& visiting)
    {
        if (computed.find(entity) != computed.end()) {
            return;
        }

        if (visiting.find(entity) != visiting.end()) {
            return;
        }

        TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform) {
            return;
        }

        visiting.insert(entity);

        XMMATRIX parentMatrix = XMMatrixIdentity();
        const EntityID parent = GetParentEntity(registry, entity, *transform);
        if (!Entity::IsNull(parent)) {
            if (TransformComponent* parentTransform = registry.GetComponent<TransformComponent>(parent)) {
                ComputeEntityWorld(registry, parent, computed, visiting);
                parentMatrix = XMLoadFloat4x4(&parentTransform->worldMatrix);
            }
        }

        transform->prevWorldMatrix = transform->worldMatrix;

        const XMMATRIX localMatrix = XMMatrixAffineTransformation(
            XMLoadFloat3(&transform->localScale),
            XMVectorZero(),
            XMLoadFloat4(&transform->localRotation),
            XMLoadFloat3(&transform->localPosition));

        const XMMATRIX worldMatrix = localMatrix * parentMatrix;
        XMStoreFloat4x4(&transform->worldMatrix, worldMatrix);

        XMVECTOR outScale, outRot, outTrans;
        if (XMMatrixDecompose(&outScale, &outRot, &outTrans, worldMatrix)) {
            XMStoreFloat3(&transform->worldPosition, outTrans);
            XMStoreFloat4(&transform->worldRotation, outRot);
            XMStoreFloat3(&transform->worldScale, outScale);
        }

        visiting.erase(entity);
        computed.insert(entity);
    }
}

void TransformSystem::Update(Registry& registry)
{
    Query<TransformComponent> query(registry);
    std::unordered_set<EntityID> computed;
    std::unordered_set<EntityID> visiting;

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        (void)trans;
        ComputeEntityWorld(registry, entity, computed, visiting);
        });
}

void TransformSystem::ComputeRecursive(EntityID entity, const XMMATRIX& parentMatrix, Registry& registry)
{
    TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
    if (!transform) return;

    transform->prevWorldMatrix = transform->worldMatrix;

    XMMATRIX localMatrix = XMMatrixAffineTransformation(
        XMLoadFloat3(&transform->localScale),
        XMVectorZero(),
        XMLoadFloat4(&transform->localRotation),
        XMLoadFloat3(&transform->localPosition)
    );

    XMMATRIX worldMatrix = localMatrix * parentMatrix;
    XMStoreFloat4x4(&transform->worldMatrix, worldMatrix);

    XMVECTOR outScale, outRot, outTrans;
    if (XMMatrixDecompose(&outScale, &outRot, &outTrans, worldMatrix)) {
        XMStoreFloat3(&transform->worldPosition, outTrans);
        XMStoreFloat4(&transform->worldRotation, outRot);
        XMStoreFloat3(&transform->worldScale, outScale);
    }

    HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
    if (hierarchy) {
        EntityID currentChild = hierarchy->firstChild;
        while (!Entity::IsNull(currentChild)) {
            ComputeRecursive(currentChild, worldMatrix, registry);

            HierarchyComponent* childHier = registry.GetComponent<HierarchyComponent>(currentChild);
            if (childHier) {
                currentChild = childHier->nextSibling;
            }
            else {
                break;
            }
        }
    }

}
