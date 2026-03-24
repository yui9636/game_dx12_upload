#include "TransformSystem.h"
#include <System\Query.h>

using namespace DirectX;

void TransformSystem::Update(Registry& registry)
{
    Query<TransformComponent, HierarchyComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans, HierarchyComponent& hierarchy) {
        if (Entity::IsNull(hierarchy.parent)) {
            ComputeRecursive(entity, XMMatrixIdentity(), registry);
        }
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
