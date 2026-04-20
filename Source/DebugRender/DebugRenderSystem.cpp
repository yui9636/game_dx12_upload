#include "DebugRenderSystem.h"
#include "Graphics.h"
#include "Gizmos.h"
#include "Collision/CollisionManager.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/MeshComponent.h"
#include "Transform/NodeAttachmentUtils.h"
#include <System\Query.h>
#include <cmath>

using namespace DirectX;

namespace
{
    float ResolveMaxWorldScale(const TransformComponent& trans)
    {
        float sx = std::fabs(trans.worldScale.x);
        float sy = std::fabs(trans.worldScale.y);
        float sz = std::fabs(trans.worldScale.z);

        float value = sx;
        if (sy > value) value = sy;
        if (sz > value) value = sz;
        if (value <= 0.0001f) value = 1.0f;
        return value;
    }

    XMFLOAT4 ToFloat4(const DirectX::SimpleMath::Vector4& value)
    {
        return { value.x, value.y, value.z, value.w };
    }
}

void DebugRenderSystem::Render(Registry& registry)
{
    auto gizmo = Graphics::Instance().GetGizmos();
    if (!gizmo) return;
    auto& collisionManager = CollisionManager::Instance();

    Query<ColliderComponent, TransformComponent> colQuery(registry);
    colQuery.ForEachWithEntity([&](EntityID entity, ColliderComponent& col, const TransformComponent& trans) {
        if (!col.drawGizmo || !col.enabled) return;

        for (auto& e : col.elements) {
            if (!e.enabled) continue;

            const XMFLOAT4 color = ToFloat4(e.color);

            if (e.registeredId != 0) {
                const Collider* runtimeCollider = collisionManager.Get(e.registeredId);
                if (runtimeCollider && runtimeCollider->enabled) {
                    if (runtimeCollider->shape == ColliderShape::Sphere) {
                        gizmo->DrawSphere(runtimeCollider->sphere.center, runtimeCollider->sphere.radius, color);
                        continue;
                    }

                    if (runtimeCollider->shape == ColliderShape::Box) {
                        gizmo->DrawBox(runtimeCollider->box.center, { 0.0f, 0.0f, 0.0f }, runtimeCollider->box.size, color);
                        continue;
                    }

                    if (runtimeCollider->shape == ColliderShape::Capsule) {
                        gizmo->DrawCapsule(runtimeCollider->capsule.base, { 0.0f, 0.0f, 0.0f }, runtimeCollider->capsule.radius, runtimeCollider->capsule.height, color);
                        continue;
                    }
                }
            }

            XMVECTOR vWorldPos;
            XMMATRIX matWorld = XMLoadFloat4x4(&trans.worldMatrix);

            if (e.nodeIndex >= 0) {
                MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
                if (mesh && mesh->model) {
                    XMFLOAT3 offset = { (float)e.offsetLocal.x, (float)e.offsetLocal.y, (float)e.offsetLocal.z };
                    XMFLOAT3 posModelSpace = NodeAttachmentUtils::GetWorldPositionNodeLocal(
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

            XMFLOAT3 worldPos;
            XMStoreFloat3(&worldPos, vWorldPos);
            const float maxWorldScale = ResolveMaxWorldScale(trans);

            if (e.type == ColliderShape::Sphere) {
                gizmo->DrawSphere(worldPos, e.radius * maxWorldScale, color);
                continue;
            }

            if (e.type == ColliderShape::Capsule) {
                gizmo->DrawCapsule(worldPos, { 0.0f, 0.0f, 0.0f }, e.radius * maxWorldScale, e.height * std::fabs(trans.worldScale.y), color);
                continue;
            }

            if (e.type == ColliderShape::Box) {
                XMFLOAT3 scaledSize = {
                    e.size.x * std::fabs(trans.worldScale.x),
                    e.size.y * std::fabs(trans.worldScale.y),
                    e.size.z * std::fabs(trans.worldScale.z)
                };
                gizmo->DrawBox(worldPos, { 0.0f, 0.0f, 0.0f }, scaledSize, color);
            }
        }
        });
}
