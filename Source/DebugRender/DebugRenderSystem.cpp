#include "DebugRenderSystem.h"
#include "Graphics.h"
#include "Gizmos.h"
#include "Collision/CollisionManager.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/MeshComponent.h"
#include "Transform/NodeAttachmentUtils.h"
#include <System/Query.h>
#include <cmath>

using namespace DirectX;

namespace
{
    // Transform の worldScale から最大軸スケールを求める。
    // Sphere や Capsule 半径の描画補正に使う。
    float ResolveMaxWorldScale(const TransformComponent& trans)
    {
        float sx = std::fabs(trans.worldScale.x);
        float sy = std::fabs(trans.worldScale.y);
        float sz = std::fabs(trans.worldScale.z);

        float value = sx;
        if (sy > value) value = sy;
        if (sz > value) value = sz;

        // 全軸とも極小なら 1.0f を返す。
        if (value <= 0.0001f) value = 1.0f;
        return value;
    }

    // SimpleMath::Vector4 を XMFLOAT4 へ変換する。
    XMFLOAT4 ToFloat4(const DirectX::SimpleMath::Vector4& value)
    {
        return { value.x, value.y, value.z, value.w };
    }
}

// コライダー Gizmo を描画する。
// まず CollisionManager に登録済みの runtime collider を優先し、
// 無ければ authoring 情報から直接 world 座標を計算して描画する。
void DebugRenderSystem::Render(Registry& registry)
{
    auto gizmo = Graphics::Instance().GetGizmos();
    if (!gizmo) return;

    auto& collisionManager = CollisionManager::Instance();

    Query<ColliderComponent, TransformComponent> colQuery(registry);
    colQuery.ForEachWithEntity([&](EntityID entity, ColliderComponent& col, const TransformComponent& trans) {
        // Gizmo 描画が無効、またはコライダー全体が無効なら何もしない。
        if (!col.drawGizmo || !col.enabled) return;

        for (auto& e : col.elements) {
            // 要素個別が無効なら描画しない。
            if (!e.enabled) continue;

            const XMFLOAT4 color = ToFloat4(e.color);

            // 既に CollisionManager に登録されている runtime collider があれば、
            // そちらの値を使って描画する。
            if (e.registeredId != 0) {
                const Collider* runtimeCollider = collisionManager.Get(e.registeredId);
                if (runtimeCollider && runtimeCollider->enabled) {
                    if (runtimeCollider->shape == ColliderShape::Sphere) {
                        gizmo->DrawSphere(
                            runtimeCollider->sphere.center,
                            runtimeCollider->sphere.radius,
                            color);
                        continue;
                    }

                    if (runtimeCollider->shape == ColliderShape::Box) {
                        gizmo->DrawBox(
                            runtimeCollider->box.center,
                            { 0.0f, 0.0f, 0.0f },
                            runtimeCollider->box.size,
                            color);
                        continue;
                    }

                    if (runtimeCollider->shape == ColliderShape::Capsule) {
                        gizmo->DrawCapsule(
                            runtimeCollider->capsule.base,
                            { 0.0f, 0.0f, 0.0f },
                            runtimeCollider->capsule.radius,
                            runtimeCollider->capsule.height,
                            color);
                        continue;
                    }
                }
            }

            // runtime collider が無い場合は、authoring 情報から world 座標を計算して描画する。
            XMVECTOR vWorldPos;
            XMMATRIX matWorld = XMLoadFloat4x4(&trans.worldMatrix);

            // nodeIndex が有効ならボーン/ノード追従位置を使う。
            if (e.nodeIndex >= 0) {
                MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);
                if (mesh && mesh->model) {
                    XMFLOAT3 offset = {
                        (float)e.offsetLocal.x,
                        (float)e.offsetLocal.y,
                        (float)e.offsetLocal.z
                    };

                    // モデルローカル空間でノード追従位置を取得し、
                    // entity の worldMatrix で world へ変換する。
                    XMFLOAT3 posModelSpace = NodeAttachmentUtils::GetWorldPositionNodeLocal(
                        mesh->model.get(), e.nodeIndex, offset);

                    vWorldPos = XMVector3TransformCoord(XMLoadFloat3(&posModelSpace), matWorld);
                }
                else {
                    // モデルが無ければ通常の local offset として扱う。
                    vWorldPos = XMVector3TransformCoord(XMLoadFloat3((XMFLOAT3*)&e.offsetLocal), matWorld);
                }
            }
            else {
                // nodeIndex 無しなら entity 基準の local offset をそのまま使う。
                vWorldPos = XMVector3TransformCoord(XMLoadFloat3((XMFLOAT3*)&e.offsetLocal), matWorld);
            }

            XMFLOAT3 worldPos;
            XMStoreFloat3(&worldPos, vWorldPos);

            const float maxWorldScale = ResolveMaxWorldScale(trans);

            // 形状ごとに適切な Gizmo を描く。
            if (e.type == ColliderShape::Sphere) {
                gizmo->DrawSphere(worldPos, e.radius * maxWorldScale, color);
                continue;
            }

            if (e.type == ColliderShape::Capsule) {
                gizmo->DrawCapsule(
                    worldPos,
                    { 0.0f, 0.0f, 0.0f },
                    e.radius * maxWorldScale,
                    e.height * std::fabs(trans.worldScale.y),
                    color);
                continue;
            }

            if (e.type == ColliderShape::Box) {
                XMFLOAT3 scaledSize = {
                    e.size.x * std::fabs(trans.worldScale.x),
                    e.size.y * std::fabs(trans.worldScale.y),
                    e.size.z * std::fabs(trans.worldScale.z)
                };

                gizmo->DrawBox(
                    worldPos,
                    { 0.0f, 0.0f, 0.0f },
                    scaledSize,
                    color);
            }
        }
        });
}