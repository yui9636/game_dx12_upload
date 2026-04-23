#include "HierarchySystem.h"
#include "Component/HierarchyComponent.h"
#include "Component/TransformComponent.h"
#include <DirectXMath.h>
#include <System/Query.h>

using namespace DirectX;

namespace
{
    // 旧実装で parent=0 を使っていた場合に備えて、
    // 0 を Entity::NULL_ID に正規化する。
    EntityID NormalizeLegacyParent(EntityID parent)
    {
        return parent == 0 ? Entity::NULL_ID : parent;
    }

    // entity の親を取得する。
    // HierarchyComponent があればそちらを優先し、
    // 無ければ TransformComponent の旧 parent 値を見る。
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

    // TransformComponent 側の親情報を同期する。
    // 旧 parent フィールドも維持している構成向け。
    void SyncTransformParent(EntityID entity, EntityID parent, Registry& registry)
    {
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            transform->parent = Entity::IsNull(parent) ? 0 : parent;
            transform->isDirty = true;
        }
    }

    // localMatrix を分解して localPosition / localRotation / localScale へ戻す。
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

// 階層更新。
// まず親が dirty な子を dirty にし、その後 dirty な transform の worldMatrix を再計算する。
void HierarchySystem::Update(Registry& registry) {
    Query<TransformComponent> query(registry);

    // 親が dirty なら子も dirty にする。
    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        const EntityID parent = GetParent(entity, registry);
        if (!Entity::IsNull(parent)) {
            auto* parentTrans = registry.GetComponent<TransformComponent>(parent);
            if (parentTrans && parentTrans->isDirty) {
                trans.isDirty = true;
            }
        }
        });

    // dirty なものだけ worldMatrix を再計算する。
    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        if (trans.isDirty) {
            ComputeWorldMatrix(entity, registry);
            trans.isDirty = false;
        }
        });
}

// entity を newParent の子にしたとき循環参照が発生するか判定する。
bool HierarchySystem::WouldCreateCycle(EntityID entity, EntityID newParent, Registry& registry)
{
    if (Entity::IsNull(entity) || Entity::IsNull(newParent)) {
        return false;
    }
    if (entity == newParent) {
        return true;
    }

    // newParent から親方向へたどって entity に到達したら循環になる。
    EntityID cursor = newParent;
    while (!Entity::IsNull(cursor)) {
        if (cursor == entity) {
            return true;
        }
        cursor = GetParent(cursor, registry);
    }
    return false;
}

// entity を現在の親子関係から切り離す。
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

    // 親の firstChild が自分なら差し替える。
    if (!Entity::IsNull(parent)) {
        if (auto* parentHierarchy = registry.GetComponent<HierarchyComponent>(parent)) {
            if (parentHierarchy->firstChild == entity) {
                parentHierarchy->firstChild = hierarchy->nextSibling;
            }
        }
    }

    // 前後 sibling のリンクをつなぎ直す。
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

    // 自身の親子リンクをクリアする。
    hierarchy->parent = Entity::NULL_ID;
    hierarchy->prevSibling = Entity::NULL_ID;
    hierarchy->nextSibling = Entity::NULL_ID;
    SyncTransformParent(entity, Entity::NULL_ID, registry);
}

// parent の子として child を付け直す。
// 実処理は Reparent に委譲する。
void HierarchySystem::AttachChild(EntityID parent, EntityID child, Registry& registry, bool keepWorldTransform)
{
    Reparent(child, parent, registry, keepWorldTransform);
}

// entity の親を newParent に付け替える。
void HierarchySystem::Reparent(EntityID entity, EntityID newParent, Registry& registry, bool keepWorldTransform)
{
    // 無効 entity は無視する。
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }
    if (!Entity::IsNull(newParent) && !registry.IsAlive(newParent)) {
        return;
    }
    if (WouldCreateCycle(entity, newParent, registry)) {
        return;
    }

    // keepWorldTransform 用に、付け替え前の worldMatrix を保持しておく。
    auto* childTransform = registry.GetComponent<TransformComponent>(entity);
    XMMATRIX childWorld = XMMatrixIdentity();
    if (childTransform) {
        childWorld = XMLoadFloat4x4(&childTransform->worldMatrix);
    }

    // 子側に HierarchyComponent が無ければ追加する。
    auto* childHierarchy = registry.GetComponent<HierarchyComponent>(entity);
    if (!childHierarchy) {
        registry.AddComponent(entity, HierarchyComponent{});
        childHierarchy = registry.GetComponent<HierarchyComponent>(entity);
    }

    // まず現在の親子関係から外す。
    Detach(entity, registry);

    // 新しい親へ接続する。
    if (!Entity::IsNull(newParent)) {
        auto* parentHierarchy = registry.GetComponent<HierarchyComponent>(newParent);
        if (!parentHierarchy) {
            registry.AddComponent(newParent, HierarchyComponent{});
            parentHierarchy = registry.GetComponent<HierarchyComponent>(newParent);
        }

        // 新しい親の firstChild の先頭へ挿入する。
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

    // world を維持したい場合は、新親基準の local を逆算し直す。
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

    // 自分以下を dirty にする。
    MarkDirtyRecursive(entity, registry);
}

// entity 以下の subtree を再帰的に dirty にする。
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

    // firstChild から sibling をたどって再帰する。
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

// entity の localTransform から worldMatrix を再計算する。
void HierarchySystem::ComputeWorldMatrix(EntityID entity, Registry& registry) {
    auto* trans = registry.GetComponent<TransformComponent>(entity);
    if (!trans) return;

    // local TRS から localMatrix を組み立てる。
    XMMATRIX local = XMMatrixScaling(trans->localScale.x, trans->localScale.y, trans->localScale.z) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&trans->localRotation)) *
        XMMatrixTranslation(trans->localPosition.x, trans->localPosition.y, trans->localPosition.z);

    XMStoreFloat4x4(&trans->localMatrix, local);

    const EntityID parent = GetParent(entity, registry);

    XMMATRIX world;

    // 親が無ければ world = local。
    if (Entity::IsNull(parent)) {
        world = local;
    }
    else {
        auto* parentTrans = registry.GetComponent<TransformComponent>(parent);
        if (parentTrans) {
            // 親が dirty なら先に親を再計算する。
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

    // worldMatrix から worldPosition / worldRotation / worldScale を分解する。
    XMVECTOR vS, vR, vT;
    if (XMMatrixDecompose(&vS, &vR, &vT, world)) {
        XMStoreFloat3(&trans->worldPosition, vT);
        XMStoreFloat4(&trans->worldRotation, vR);
        XMStoreFloat3(&trans->worldScale, vS);
    }
}