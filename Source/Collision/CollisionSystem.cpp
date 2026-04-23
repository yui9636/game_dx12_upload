#include "CollisionSystem.h"

#include "Collision/CollisionManager.h"
#include "Transform/NodeAttachmentUtils.h"
#include <System/Query.h>
#include <cmath>

using namespace DirectX;

namespace
{
    // 各軸スケール値を解決する。
    // 値が極小なら fallback を使い、それも極小なら 1.0f を返す。
    float ResolveAxisScale(float value, float fallback)
    {
        const float absValue = std::fabs(value);
        if (absValue > 0.0001f) {
            return absValue;
        }
        return fallback > 0.0001f ? fallback : 1.0f;
    }

    // Transform の worldScale から、最大軸スケールを取得する。
    // Sphere や Capsule 半径の補正に使う。
    float ResolveMaxWorldScale(const TransformComponent& transform)
    {
        float value = std::fabs(transform.worldScale.x);

        const float sy = std::fabs(transform.worldScale.y);
        if (sy > value) {
            value = sy;
        }

        const float sz = std::fabs(transform.worldScale.z);
        if (sz > value) {
            value = sz;
        }

        // 全軸とも極小なら 1.0f を返す。
        if (value <= 0.0001f) {
            value = 1.0f;
        }
        return value;
    }

    // コライダー要素の world 座標を求める。
    // nodeIndex が有効ならボーン/ノード追従位置を使い、無効なら entity 基準の local offset を使う。
    XMFLOAT3 ResolveColliderWorldPosition(
        Registry& registry,
        EntityID entity,
        const ColliderComponent::Element& element,
        const TransformComponent& transform)
    {
        const XMMATRIX worldMatrix = XMLoadFloat4x4(&transform.worldMatrix);

        // 要素の local offset を XMFLOAT3 に変換する。
        const XMFLOAT3 localOffset = {
            element.offsetLocal.x,
            element.offsetLocal.y,
            element.offsetLocal.z
        };

        XMVECTOR worldPosition = XMVectorZero();

        // ノード指定ありなら、そのノードの位置を使って world 化する。
        if (element.nodeIndex >= 0) {
            MeshComponent* mesh = registry.GetComponent<MeshComponent>(entity);

            if (mesh && mesh->model) {
                // モデルの指定ノード上での local offset を取得し、entity の worldMatrix へ変換する。
                const XMFLOAT3 nodeLocalPosition =
                    NodeAttachmentUtils::GetWorldPositionNodeLocal(
                        mesh->model.get(),
                        element.nodeIndex,
                        localOffset);

                worldPosition = XMVector3TransformCoord(XMLoadFloat3(&nodeLocalPosition), worldMatrix);
            }
            else {
                // モデルが無い場合は通常の local offset として扱う。
                worldPosition = XMVector3TransformCoord(XMLoadFloat3(&localOffset), worldMatrix);
            }
        }
        else {
            // ノード指定なしなら entity 基準の local offset を world 化する。
            worldPosition = XMVector3TransformCoord(XMLoadFloat3(&localOffset), worldMatrix);
        }

        XMFLOAT3 result{};
        XMStoreFloat3(&result, worldPosition);
        return result;
    }

    // Sphere コライダーを CollisionManager 上で更新または再登録する。
    void RefreshSphereCollider(
        CollisionManager& collisionManager,
        EntityID entity,
        ColliderComponent::Element& element,
        const SphereDesc& desc)
    {
        // 既に登録済みなら更新を試みる。
        if (element.registeredId != 0) {
            if (!collisionManager.UpdateSphere(element.registeredId, desc)) {
                // 更新失敗なら一度削除して再登録できる状態へ戻す。
                collisionManager.Remove(element.registeredId);
                element.registeredId = 0;
            }
        }

        // 未登録なら新規登録する。
        if (element.registeredId == 0) {
            element.registeredId = collisionManager.AddSphere(
                desc,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                element.attribute);
        }

        // 登録成功後は有効化と userPtr 再設定を行う。
        if (element.registeredId != 0) {
            collisionManager.SetEnabled(element.registeredId, true);
            collisionManager.SetUserPtr(
                element.registeredId,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
        }
    }

    // Capsule コライダーを CollisionManager 上で更新または再登録する。
    void RefreshCapsuleCollider(
        CollisionManager& collisionManager,
        EntityID entity,
        ColliderComponent::Element& element,
        const CapsuleDesc& desc)
    {
        // 既に登録済みなら更新を試みる。
        if (element.registeredId != 0) {
            if (!collisionManager.UpdateCapsule(element.registeredId, desc)) {
                // 更新失敗なら一度削除して再登録できる状態へ戻す。
                collisionManager.Remove(element.registeredId);
                element.registeredId = 0;
            }
        }

        // 未登録なら新規登録する。
        if (element.registeredId == 0) {
            element.registeredId = collisionManager.AddCapsule(
                desc,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                element.attribute);
        }

        // 登録成功後は有効化と userPtr 再設定を行う。
        if (element.registeredId != 0) {
            collisionManager.SetEnabled(element.registeredId, true);
            collisionManager.SetUserPtr(
                element.registeredId,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
        }
    }

    // Box コライダーを CollisionManager 上で更新または再登録する。
    void RefreshBoxCollider(
        CollisionManager& collisionManager,
        EntityID entity,
        ColliderComponent::Element& element,
        const BoxDesc& desc)
    {
        // 既に登録済みなら更新を試みる。
        if (element.registeredId != 0) {
            if (!collisionManager.UpdateBox(element.registeredId, desc)) {
                // 更新失敗なら一度削除して再登録できる状態へ戻す。
                collisionManager.Remove(element.registeredId);
                element.registeredId = 0;
            }
        }

        // 未登録なら新規登録する。
        if (element.registeredId == 0) {
            element.registeredId = collisionManager.AddBox(
                desc,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)),
                element.attribute);
        }

        // 登録成功後は有効化と userPtr 再設定を行う。
        if (element.registeredId != 0) {
            collisionManager.SetEnabled(element.registeredId, true);
            collisionManager.SetUserPtr(
                element.registeredId,
                reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
        }
    }
}

// 毎フレーム、ColliderComponent の内容を CollisionManager へ反映する。
void CollisionSystem::Update(Registry& registry)
{
    auto& collisionManager = CollisionManager::Instance();

    Query<ColliderComponent, TransformComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, ColliderComponent& collider, TransformComponent& transform) {
        // entity 側コライダー全体が無効なら、登録済みコライダーを無効化する。
        if (!collider.enabled) {
            for (auto& element : collider.elements) {
                // runtime 専用要素はここでは触らない。
                if (element.runtimeTag != 0) {
                    continue;
                }

                if (element.registeredId != 0) {
                    collisionManager.SetEnabled(element.registeredId, false);
                }
            }
            return;
        }

        // worldScale から各形状へ使うスケール値を解決する。
        const float maxWorldScale = ResolveMaxWorldScale(transform);
        const float scaleX = ResolveAxisScale(transform.worldScale.x, maxWorldScale);
        const float scaleY = ResolveAxisScale(transform.worldScale.y, maxWorldScale);
        const float scaleZ = ResolveAxisScale(transform.worldScale.z, maxWorldScale);

        for (auto& element : collider.elements) {
            // runtime 専用要素はここでは管理しない。
            if (element.runtimeTag != 0) {
                continue;
            }

            // 要素個別が無効なら manager 側も無効化する。
            if (!element.enabled) {
                if (element.registeredId != 0) {
                    collisionManager.SetEnabled(element.registeredId, false);
                }
                continue;
            }

            // 現在の element の world 位置を求める。
            const XMFLOAT3 worldPosition =
                ResolveColliderWorldPosition(registry, entity, element, transform);

            // 形状ごとに manager 側へ更新反映する。
            switch (element.type) {
            case ColliderShape::Sphere:
                RefreshSphereCollider(
                    collisionManager,
                    entity,
                    element,
                    { worldPosition, element.radius * maxWorldScale });
                break;

            case ColliderShape::Capsule:
                RefreshCapsuleCollider(
                    collisionManager,
                    entity,
                    element,
                    { worldPosition, element.radius * maxWorldScale, element.height * scaleY });
                break;

            case ColliderShape::Box:
                RefreshBoxCollider(
                    collisionManager,
                    entity,
                    element,
                    {
                        worldPosition,
                        {
                            element.size.x * scaleX,
                            element.size.y * scaleY,
                            element.size.z * scaleZ
                        }
                    });
                break;
            }
        }
        });
}

// 終了時やシーン切り替え時に、登録済みコライダーを CollisionManager から外す。
void CollisionSystem::Finalize(Registry& registry)
{
    Query<ColliderComponent> query(registry);
    query.ForEach([](ColliderComponent& collider) {
        for (auto& element : collider.elements) {
            if (element.registeredId != 0) {
                CollisionManager::Instance().Remove(element.registeredId);
                element.registeredId = 0;
            }
        }
        });
}