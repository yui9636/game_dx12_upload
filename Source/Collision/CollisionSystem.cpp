#include "CollisionSystem.h"
#include "Collision/CollisionManager.h"
#include "Component/NodeAttachComponent.h"
#include <System\Query.h>

using namespace DirectX;

void CollisionSystem::Update(Registry& registry)
{
    auto& colMgr = CollisionManager::Instance();

    Query<ColliderComponent, TransformComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, ColliderComponent& col, TransformComponent& trans) {

        if (!col.enabled) {
            for (auto& e : col.elements) {
                if (e.registeredId != 0) colMgr.SetEnabled(e.registeredId, false);
            }
            return;
        }

        for (auto& e : col.elements) {
            if (!e.enabled) {
                if (e.registeredId != 0) colMgr.SetEnabled(e.registeredId, false);
                continue;
            }

            XMVECTOR vWorldPos;
            XMMATRIX matWorld = XMLoadFloat4x4(&trans.worldMatrix);

            if (e.nodeIndex >= 0) {
                // RegistryからMeshを直接検索
                MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
                if (mesh && mesh->model) {
                    XMFLOAT3 offset = { (float)e.offsetLocal.x, (float)e.offsetLocal.y, (float)e.offsetLocal.z };
                    XMFLOAT3 posModelSpace = NodeAttachComponent::GetWorldPosition_NodeLocal(
                        mesh->model.get(), e.nodeIndex, offset);
                    vWorldPos = XMVector3TransformCoord(XMLoadFloat3(&posModelSpace), matWorld);
                }
                else {
                    vWorldPos = XMVector3TransformCoord(XMLoadFloat3((XMFLOAT3*)&e.offsetLocal), matWorld);
                }
            }
            else {
                vWorldPos = XMVector3TransformCoord(XMLoadFloat3((XMFLOAT3*)&e.offsetLocal), matWorld);
            }

            // （以下、登録・更新処理は元のコードと同じ。
            // 　ただし e.registeredId などの設定があるため、そのまま使用）
            /* ... (省略) ... */
        }
        });
}

void CollisionSystem::Finalize(Registry& registry) {
    Query<ColliderComponent> query(registry);
    query.ForEach([](ColliderComponent& col) {
        for (auto& e : col.elements) {
            if (e.registeredId != 0) {
                CollisionManager::Instance().Remove(e.registeredId);
                e.registeredId = 0;
            }
        }
        });
}