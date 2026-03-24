#include "HierarchySystem.h"
#include "Component/TransformComponent.h"
#include <DirectXMath.h>
#include <System\Query.h>

using namespace DirectX;

void HierarchySystem::Update(Registry& registry) {
    Query<TransformComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans) {
        if (trans.parent != 0) {
            auto* parentTrans = registry.GetComponent<TransformComponent>(trans.parent);
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

void HierarchySystem::ComputeWorldMatrix(EntityID entity, Registry& registry) {
    auto* trans = registry.GetComponent<TransformComponent>(entity);
    if (!trans) return;

    XMMATRIX local = XMMatrixScaling(trans->localScale.x, trans->localScale.y, trans->localScale.z) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&trans->localRotation)) *
        XMMatrixTranslation(trans->localPosition.x, trans->localPosition.y, trans->localPosition.z);

    XMStoreFloat4x4(&trans->localMatrix, local);

    XMMATRIX world;
    if (trans->parent == 0) {
        world = local;
    }
    else {
        auto* parentTrans = registry.GetComponent<TransformComponent>(trans->parent);
        if (parentTrans) {
            if (parentTrans->isDirty) {
                ComputeWorldMatrix(trans->parent, registry);
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
