#include "TransformSystem.h"
#include <System\Query.h>

using namespace DirectX;

void TransformSystem::Update(Registry& registry)
{
    Query<TransformComponent, HierarchyComponent> query(registry);

    // 親がNull（自分がルートノード）のものだけを起点にする
    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans, HierarchyComponent& hierarchy) {
        if (Entity::IsNull(hierarchy.parent)) {
            ComputeRecursive(entity, XMMatrixIdentity(), registry);
        }
        });
}

void TransformSystem::ComputeRecursive(EntityID entity, const XMMATRIX& parentMatrix, Registry& registry)
{
    // Registry から直接コンポーネントを取得
    TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
    if (!transform) return;

    transform->prevWorldMatrix = transform->worldMatrix;

    // --- [ 1. アフィーン変換行列の一括生成 ] ---
    XMMATRIX localMatrix = XMMatrixAffineTransformation(
        XMLoadFloat3(&transform->localScale),
        XMVectorZero(),
        XMLoadFloat4(&transform->localRotation),
        XMLoadFloat3(&transform->localPosition)
    );

    // --- [ 2. 親の行列を掛けてワールド行列を確定 ] ---
    XMMATRIX worldMatrix = localMatrix * parentMatrix;
    XMStoreFloat4x4(&transform->worldMatrix, worldMatrix);

    // --- [ 3. 行列の分解とキャッシュ ] ---
    XMVECTOR outScale, outRot, outTrans;
    if (XMMatrixDecompose(&outScale, &outRot, &outTrans, worldMatrix)) {
        XMStoreFloat3(&transform->worldPosition, outTrans);
        XMStoreFloat4(&transform->worldRotation, outRot);
        XMStoreFloat3(&transform->worldScale, outScale);
    }

    // --- [ 4. 子への再帰更新 ] ---
    HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
    if (hierarchy) {
        // 長男から始めて、次男、三男と辿っていく
        EntityID currentChild = hierarchy->firstChild;
        while (!Entity::IsNull(currentChild)) {
            ComputeRecursive(currentChild, worldMatrix, registry);

            HierarchyComponent* childHier = registry.GetComponent<HierarchyComponent>(currentChild);
            if (childHier) {
                currentChild = childHier->nextSibling; // 次の兄弟へ
            }
            else {
                break;
            }
        }
    }

}