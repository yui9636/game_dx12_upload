#include "DebugRenderSystem.h"
#include "Graphics.h"
#include "Gizmos.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NodeAttachComponent.h"
#include <System\Query.h>

using namespace DirectX;

void DebugRenderSystem::Render(Registry& registry)
{
    auto gizmo = Graphics::Instance().GetGizmos();
    if (!gizmo) return;

    Query<ColliderComponent, TransformComponent> colQuery(registry);
    colQuery.ForEachWithEntity([&](EntityID entity, ColliderComponent& col, const TransformComponent& trans) {
        if (!col.drawGizmo || !col.enabled) return;

        for (auto& e : col.elements) {
            if (!e.enabled) continue;

            XMVECTOR vWorldPos;
            XMMATRIX matWorld = XMLoadFloat4x4(&trans.worldMatrix);

            if (e.nodeIndex >= 0) {
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

        }
        });

    Query<LightComponent, TransformComponent> lightQuery(registry);
    lightQuery.ForEach([&](const LightComponent& light, const TransformComponent& trans) {
        XMFLOAT4 iconCol = { light.color.x, light.color.y, light.color.z, 1.0f };
        if (light.type == LightType::Directional) {
            gizmo->DrawSphere(trans.worldPosition, 0.5f, iconCol);
        }
        else {
            gizmo->DrawSphere(trans.worldPosition, 0.2f, iconCol);
        }
        });
}
