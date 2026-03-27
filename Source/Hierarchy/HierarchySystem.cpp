#include "HierarchySystem.h"
#include "Component/HierarchyComponent.h"
#include "Component/TransformComponent.h"
#include <DirectXMath.h>
#include <System/Query.h>

using namespace DirectX;

namespace
{
    EntityID NormalizeLegacyParent(EntityID parent)
    {
        return parent == 0 ? Entity::NULL_ID : parent;
    }

    EntityID GetParent(EntityID entity, Registry& registry)
    {
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            return NormalizeLegacyParent(transform->parent);
        }
        return Entity::NULL_ID;
    }

    void SyncTransformParent(EntityID entity, EntityID parent, Registry& registry)
    {
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            transform->parent = Entity::IsNull(parent) ? 0 : parent;
            transform->isDirty = true;
        }
    }

    void DecomposeToLocal(const XMMATRIX& localMatrix, TransformComponent& transform)
    {
        XMVECTOR scale;
        XMVECTOR rotation;
        XMVECTOR translation;
        if (XMMatrixDecompose(&scale, &rotation, &translation, localMatrix)) {
            XMStoreFloat3(&transform.localScale, scale);
            XMStoreFloat4(&transform.localRotation, rotation);
            XMStoreFloat3(&transform.localPosition, translation);
        }
    }
}

void HierarchySystem::Update(Registry& registry) {
    Query<TransformComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        const EntityID parent = GetParent(entity, registry);
        if (!Entity::IsNull(parent)) {
            auto* parentTrans = registry.GetComponent<TransformComponent>(parent);
            if (parentTrans && parentTrans->isDirty) {
                trans.isDirty = true;
            }
        }
    });

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        if (trans.isDirty) {
            ComputeWorldMatrix(entity, registry);
            trans.isDirty = false;
        }
    });
}

bool HierarchySystem::WouldCreateCycle(EntityID entity, EntityID newParent, Registry& registry)
{
    if (Entity::IsNull(entity) || Entity::IsNull(newParent)) {
        return false;
    }
    if (entity == newParent) {
        return true;
    }

    EntityID cursor = newParent;
    while (!Entity::IsNull(cursor)) {
        if (cursor == entity) {
            return true;
        }
        cursor = GetParent(cursor, registry);
    }
    return false;
}

void HierarchySystem::Detach(EntityID entity, Registry& registry)
{
    if (Entity::IsNull(entity)) {
        return;
    }

    auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
    if (!hierarchy) {
        SyncTransformParent(entity, Entity::NULL_ID, registry);
        return;
    }

    const EntityID parent = hierarchy->parent;
    if (!Entity::IsNull(parent)) {
        if (auto* parentHierarchy = registry.GetComponent<HierarchyComponent>(parent)) {
            if (parentHierarchy->firstChild == entity) {
                parentHierarchy->firstChild = hierarchy->nextSibling;
            }
        }
    }

    if (!Entity::IsNull(hierarchy->prevSibling)) {
        if (auto* prevHierarchy = registry.GetComponent<HierarchyComponent>(hierarchy->prevSibling)) {
            prevHierarchy->nextSibling = hierarchy->nextSibling;
        }
    }
    if (!Entity::IsNull(hierarchy->nextSibling)) {
        if (auto* nextHierarchy = registry.GetComponent<HierarchyComponent>(hierarchy->nextSibling)) {
            nextHierarchy->prevSibling = hierarchy->prevSibling;
        }
    }

    hierarchy->parent = Entity::NULL_ID;
    hierarchy->prevSibling = Entity::NULL_ID;
    hierarchy->nextSibling = Entity::NULL_ID;
    SyncTransformParent(entity, Entity::NULL_ID, registry);
}

void HierarchySystem::AttachChild(EntityID parent, EntityID child, Registry& registry, bool keepWorldTransform)
{
    Reparent(child, parent, registry, keepWorldTransform);
}

void HierarchySystem::Reparent(EntityID entity, EntityID newParent, Registry& registry, bool keepWorldTransform)
{
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }
    if (!Entity::IsNull(newParent) && !registry.IsAlive(newParent)) {
        return;
    }
    if (WouldCreateCycle(entity, newParent, registry)) {
        return;
    }

    auto* childTransform = registry.GetComponent<TransformComponent>(entity);
    XMMATRIX childWorld = XMMatrixIdentity();
    if (childTransform) {
        childWorld = XMLoadFloat4x4(&childTransform->worldMatrix);
    }

    auto* childHierarchy = registry.GetComponent<HierarchyComponent>(entity);
    if (!childHierarchy) {
        registry.AddComponent(entity, HierarchyComponent{});
        childHierarchy = registry.GetComponent<HierarchyComponent>(entity);
    }

    Detach(entity, registry);

    if (!Entity::IsNull(newParent)) {
        auto* parentHierarchy = registry.GetComponent<HierarchyComponent>(newParent);
        if (!parentHierarchy) {
            registry.AddComponent(newParent, HierarchyComponent{});
            parentHierarchy = registry.GetComponent<HierarchyComponent>(newParent);
        }

        childHierarchy->parent = newParent;
        childHierarchy->prevSibling = Entity::NULL_ID;
        childHierarchy->nextSibling = parentHierarchy->firstChild;
        if (!Entity::IsNull(parentHierarchy->firstChild)) {
            if (auto* oldFirst = registry.GetComponent<HierarchyComponent>(parentHierarchy->firstChild)) {
                oldFirst->prevSibling = entity;
            }
        }
        parentHierarchy->firstChild = entity;
    }

    SyncTransformParent(entity, newParent, registry);

    if (keepWorldTransform && childTransform) {
        XMMATRIX local = childWorld;
        if (!Entity::IsNull(newParent)) {
            if (auto* parentTransform = registry.GetComponent<TransformComponent>(newParent)) {
                const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
                local = childWorld * XMMatrixInverse(nullptr, parentWorld);
            }
        }
        DecomposeToLocal(local, *childTransform);
    }

    MarkDirtyRecursive(entity, registry);
}

void HierarchySystem::MarkDirtyRecursive(EntityID entity, Registry& registry)
{
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }

    if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
        transform->isDirty = true;
    }

    auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
    if (!hierarchy) {
        return;
    }

    EntityID child = hierarchy->firstChild;
    while (!Entity::IsNull(child)) {
        EntityID next = Entity::NULL_ID;
        if (auto* childHierarchy = registry.GetComponent<HierarchyComponent>(child)) {
            next = childHierarchy->nextSibling;
        }
        MarkDirtyRecursive(child, registry);
        child = next;
    }
}

void HierarchySystem::ComputeWorldMatrix(EntityID entity, Registry& registry) {
    auto* trans = registry.GetComponent<TransformComponent>(entity);
    if (!trans) return;

    XMMATRIX local = XMMatrixScaling(trans->localScale.x, trans->localScale.y, trans->localScale.z) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&trans->localRotation)) *
        XMMatrixTranslation(trans->localPosition.x, trans->localPosition.y, trans->localPosition.z);

    XMStoreFloat4x4(&trans->localMatrix, local);

    const EntityID parent = GetParent(entity, registry);

    XMMATRIX world;
    if (Entity::IsNull(parent)) {
        world = local;
    }
    else {
        auto* parentTrans = registry.GetComponent<TransformComponent>(parent);
        if (parentTrans) {
            if (parentTrans->isDirty) {
                ComputeWorldMatrix(parent, registry);
                parentTrans->isDirty = false;
            }
            world = local * XMLoadFloat4x4(&parentTrans->worldMatrix);
        }
        else {
            world = local;
        }
    }
    XMStoreFloat4x4(&trans->worldMatrix, world);

    XMVECTOR vS, vR, vT;
    if (XMMatrixDecompose(&vS, &vR, &vT, world)) {
        XMStoreFloat3(&trans->worldPosition, vT);
        XMStoreFloat4(&trans->worldRotation, vR);
        XMStoreFloat3(&trans->worldScale, vS);
    }
}
