#include "CollisionSystem.h"

#include "Collision/CollisionManager.h"
#include "Component/HierarchyComponent.h"
#include "Gameplay/CharacterPhysicsComponent.h"
#include "Transform/TransformSystem.h"
#include "Transform/NodeAttachmentUtils.h"
#include <System/Query.h>
#include <algorithm>
#include <cmath>
#include <vector>

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
    EntityID UserPtrToEntity(void* userPtr)
    {
        return static_cast<EntityID>(reinterpret_cast<uintptr_t>(userPtr));
    }

    EntityID GetTransformParent(Registry& registry, EntityID entity, const TransformComponent& transform)
    {
        if (HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }
        return transform.parent == 0 ? Entity::NULL_ID : transform.parent;
    }

    XMFLOAT3 ResolveColliderCenter(const Collider& collider)
    {
        switch (collider.shape) {
        case ColliderShape::Sphere:
            return collider.sphere.center;
        case ColliderShape::Capsule:
            return {
                collider.capsule.base.x,
                collider.capsule.base.y + collider.capsule.height * 0.5f,
                collider.capsule.base.z
            };
        case ColliderShape::Box:
            return collider.box.center;
        }
        return {};
    }

    struct BodyPairResolution
    {
        EntityID first = Entity::NULL_ID;
        EntityID second = Entity::NULL_ID;
        float dirX = 1.0f;
        float dirZ = 0.0f;
        float depth = 0.0f;
    };

    void AddOrUpdateBodyPair(
        std::vector<BodyPairResolution>& pairs,
        EntityID entityA,
        EntityID entityB,
        float dirAX,
        float dirAZ,
        float depth)
    {
        if (depth <= 0.0001f) {
            return;
        }

        const EntityID first = (std::min)(entityA, entityB);
        const EntityID second = (std::max)(entityA, entityB);
        if (first == second) {
            return;
        }

        float dirFirstX = dirAX;
        float dirFirstZ = dirAZ;
        if (first != entityA) {
            dirFirstX = -dirFirstX;
            dirFirstZ = -dirFirstZ;
        }

        for (auto& pair : pairs) {
            if (pair.first == first && pair.second == second) {
                if (depth > pair.depth) {
                    pair.dirX = dirFirstX;
                    pair.dirZ = dirFirstZ;
                    pair.depth = depth;
                }
                return;
            }
        }

        BodyPairResolution pair;
        pair.first = first;
        pair.second = second;
        pair.dirX = dirFirstX;
        pair.dirZ = dirFirstZ;
        pair.depth = depth;
        pairs.push_back(pair);
    }

    void TranslateTransformWorldHorizontal(
        Registry& registry,
        EntityID entity,
        TransformComponent& transform,
        float deltaX,
        float deltaZ)
    {
        if (std::fabs(deltaX) <= 0.00001f && std::fabs(deltaZ) <= 0.00001f) {
            return;
        }

        transform.worldPosition.x += deltaX;
        transform.worldPosition.z += deltaZ;

        const EntityID parent = GetTransformParent(registry, entity, transform);
        TransformComponent* parentTransform = !Entity::IsNull(parent)
            ? registry.GetComponent<TransformComponent>(parent)
            : nullptr;

        if (parentTransform) {
            const XMVECTOR worldPos = XMLoadFloat3(&transform.worldPosition);
            const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
            const XMVECTOR localPos = XMVector3TransformCoord(
                worldPos,
                XMMatrixInverse(nullptr, parentWorld));
            XMStoreFloat3(&transform.localPosition, localPos);
        }
        else {
            transform.localPosition.x += deltaX;
            transform.localPosition.z += deltaZ;
        }

        transform.isDirty = true;
    }

    bool ResolveBodyBodyPushback(Registry& registry, CollisionManager& collisionManager)
    {
        std::vector<CollisionContact> contacts;
        collisionManager.ComputeAllContacts(contacts);
        if (contacts.empty()) {
            return false;
        }

        std::vector<BodyPairResolution> pairs;
        pairs.reserve(contacts.size());

        for (const auto& contact : contacts) {
            const Collider* colliderA = collisionManager.Get(contact.idA);
            const Collider* colliderB = collisionManager.Get(contact.idB);
            if (!colliderA || !colliderB) {
                continue;
            }
            if (!colliderA->enabled || !colliderB->enabled) {
                continue;
            }
            if (colliderA->attribute != ColliderAttribute::Body ||
                colliderB->attribute != ColliderAttribute::Body) {
                continue;
            }

            const EntityID entityA = UserPtrToEntity(colliderA->userPtr);
            const EntityID entityB = UserPtrToEntity(colliderB->userPtr);
            if (Entity::IsNull(entityA) || Entity::IsNull(entityB) || entityA == entityB) {
                continue;
            }

            const bool movableA = registry.GetComponent<CharacterPhysicsComponent>(entityA) != nullptr;
            const bool movableB = registry.GetComponent<CharacterPhysicsComponent>(entityB) != nullptr;
            if (!movableA && !movableB) {
                continue;
            }

            const XMFLOAT3 centerA = ResolveColliderCenter(*colliderA);
            const XMFLOAT3 centerB = ResolveColliderCenter(*colliderB);
            float dirX = centerB.x - centerA.x;
            float dirZ = centerB.z - centerA.z;
            float lenSq = dirX * dirX + dirZ * dirZ;

            if (lenSq <= 0.000001f) {
                const auto* transformA = registry.GetComponent<TransformComponent>(entityA);
                const auto* transformB = registry.GetComponent<TransformComponent>(entityB);
                if (transformA && transformB) {
                    dirX = transformB->worldPosition.x - transformA->worldPosition.x;
                    dirZ = transformB->worldPosition.z - transformA->worldPosition.z;
                    lenSq = dirX * dirX + dirZ * dirZ;
                }
            }

            if (lenSq <= 0.000001f) {
                dirX = 1.0f;
                dirZ = 0.0f;
            }
            else {
                const float invLen = 1.0f / std::sqrt(lenSq);
                dirX *= invLen;
                dirZ *= invLen;
            }

            AddOrUpdateBodyPair(
                pairs,
                entityA,
                entityB,
                dirX,
                dirZ,
                contact.hit.penetrationDepth);
        }

        bool movedAny = false;
        for (const auto& pair : pairs) {
            TransformComponent* firstTransform = registry.GetComponent<TransformComponent>(pair.first);
            TransformComponent* secondTransform = registry.GetComponent<TransformComponent>(pair.second);
            if (!firstTransform || !secondTransform) {
                continue;
            }

            const bool firstMovable = registry.GetComponent<CharacterPhysicsComponent>(pair.first) != nullptr;
            const bool secondMovable = registry.GetComponent<CharacterPhysicsComponent>(pair.second) != nullptr;

            if (firstMovable && secondMovable) {
                const float halfDepth = pair.depth * 0.5f;
                TranslateTransformWorldHorizontal(
                    registry,
                    pair.first,
                    *firstTransform,
                    -pair.dirX * halfDepth,
                    -pair.dirZ * halfDepth);
                TranslateTransformWorldHorizontal(
                    registry,
                    pair.second,
                    *secondTransform,
                    pair.dirX * halfDepth,
                    pair.dirZ * halfDepth);
                movedAny = true;
            }
            else if (firstMovable) {
                TranslateTransformWorldHorizontal(
                    registry,
                    pair.first,
                    *firstTransform,
                    -pair.dirX * pair.depth,
                    -pair.dirZ * pair.depth);
                movedAny = true;
            }
            else if (secondMovable) {
                TranslateTransformWorldHorizontal(
                    registry,
                    pair.second,
                    *secondTransform,
                    pair.dirX * pair.depth,
                    pair.dirZ * pair.depth);
                movedAny = true;
            }
        }

        return movedAny;
    }
}

// 毎フレーム、ColliderComponent の内容を CollisionManager へ反映する。
void CollisionSystem::Update(Registry& registry)
{
    auto& collisionManager = CollisionManager::Instance();

    auto syncPersistentColliders = [&]() {
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
    };

    syncPersistentColliders();

    if (ResolveBodyBodyPushback(registry, collisionManager)) {
        TransformSystem transformSystem;
        transformSystem.Update(registry);
        syncPersistentColliders();
    }
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