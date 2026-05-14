#include "LockOnSystem.h"

#include "Component/CameraBehaviorComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/LockOnTargetComponent.h"
#include "Gameplay/LocomotionStateComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Input/InputActionMapComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <DirectXMath.h>
#include <cmath>
#include <limits>

namespace
{
    int FindActionIndex(const InputActionMapComponent& map, const char* name)
    {
        const auto& actions = map.asset.actions;
        for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
            if (actions[i].actionName == name) return i;
        }
        return -1;
    }

    bool IsActionRisingEdge(const ResolvedInputStateComponent& input, int actionIndex)
    {
        if (actionIndex < 0 || actionIndex >= ResolvedInputStateComponent::MAX_ACTIONS) return false;
        return input.actions[actionIndex].pressed;
    }

    EntityID FindCameraEntity(Registry& registry)
    {
        EntityID found = Entity::NULL_ID;
        Query<CameraTPVControlComponent, TransformComponent> q(registry);
        q.ForEachWithEntity([&](EntityID e, CameraTPVControlComponent&, TransformComponent&) {
            if (Entity::IsNull(found)) found = e;
        });
        return found;
    }

    EntityID PickClosestEnemy(Registry& registry,
                              const DirectX::XMFLOAT3& fromPos,
                              const DirectX::XMFLOAT3& cameraForward,
                              float maxRange,
                              float fovRadians)
    {
        const float maxRangeSq = maxRange * maxRange;
        const float cosHalfFov = std::cos(fovRadians * 0.5f);

        EntityID best = Entity::NULL_ID;
        float bestScore = std::numeric_limits<float>::max();

        Query<EnemyTagComponent, TransformComponent> q(registry);
        q.ForEachWithEntity([&](EntityID e, EnemyTagComponent&, TransformComponent& tr) {
            // Health を持つ敵が死亡済みなら除外する。
            if (auto* h = registry.GetComponent<HealthComponent>(e)) {
                if (h->isDead || h->health <= 0) return;
            }

            const float dx = tr.worldPosition.x - fromPos.x;
            const float dy = tr.worldPosition.y - fromPos.y;
            const float dz = tr.worldPosition.z - fromPos.z;
            const float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq <= 0.0001f || distSq > maxRangeSq) return;

            // カメラ前方を基準にした単純な平面 FOV（XZ 平面）。
            const float horizLen = std::sqrt(dx * dx + dz * dz);
            float dot = 0.0f;
            if (horizLen > 0.0001f) {
                const float fwdLen =
                    std::sqrt(cameraForward.x * cameraForward.x + cameraForward.z * cameraForward.z);
                if (fwdLen > 0.0001f) {
                    dot = (dx * cameraForward.x + dz * cameraForward.z) / (horizLen * fwdLen);
                }
            }
            if (dot < cosHalfFov) return;

            // スコア: 距離に小さな FOV バイアスを加える（同点なら中心に近いほうが勝つ）。
            const float score = distSq * (2.0f - dot);
            if (score < bestScore) {
                bestScore = score;
                best = e;
            }
        });
        return best;
    }
}

void LockOnSystem::Update(Registry& registry, float /*dt*/)
{
    const EntityID cameraEntity = FindCameraEntity(registry);
    if (Entity::IsNull(cameraEntity)) return;
    auto* cameraTPV = registry.GetComponent<CameraTPVControlComponent>(cameraEntity);
    auto* cameraTr  = registry.GetComponent<TransformComponent>(cameraEntity);
    if (!cameraTPV || !cameraTr) return;

    Query<PlayerTagComponent, LockOnTargetComponent, TransformComponent, ResolvedInputStateComponent, InputActionMapComponent> q(registry);
    q.ForEachWithEntity([&](EntityID playerEntity,
                            PlayerTagComponent&,
                            LockOnTargetComponent& lock,
                            TransformComponent& playerTr,
                            ResolvedInputStateComponent& input,
                            InputActionMapComponent& map) {
        const int lockOnIndex = FindActionIndex(map, "LockOn");
        const bool toggleRequested = IsActionRisingEdge(input, lockOnIndex);

        // 古いロック対象（破棄済み / 死亡済み）を落とす。
        if (!Entity::IsNull(lock.currentTarget)) {
            bool stillValid = registry.IsAlive(lock.currentTarget);
            if (stillValid) {
                if (auto* h = registry.GetComponent<HealthComponent>(lock.currentTarget)) {
                    if (h->isDead || h->health <= 0) stillValid = false;
                }
            }
            if (!stillValid) {
                lock.currentTarget = Entity::NULL_ID;
                cameraTPV->target = playerEntity;
            }
        }

        if (toggleRequested) {
            if (Entity::IsNull(lock.currentTarget)) {
                // 取得。
                DirectX::XMFLOAT3 forward{ 0.0f, 0.0f, 1.0f };
                forward.x = cameraTr->worldMatrix._31;
                forward.y = cameraTr->worldMatrix._32;
                forward.z = cameraTr->worldMatrix._33;
                if (std::fabs(forward.x) + std::fabs(forward.z) < 0.0001f) {
                    // カメラ行列が未準備ならプレイヤーの向きへフォールバックする。
                    forward = { std::sin(playerTr.localRotation.y), 0.0f, std::cos(playerTr.localRotation.y) };
                }
                const EntityID picked = PickClosestEnemy(
                    registry, playerTr.worldPosition, forward, lock.maxRange, lock.fovRadians);
                if (!Entity::IsNull(picked)) {
                    lock.currentTarget = picked;
                    cameraTPV->target = picked;
                }
            } else {
                // 解除。
                lock.currentTarget = Entity::NULL_ID;
                cameraTPV->target = playerEntity;
            }
        }

        // ロックオン中は対象のほうへプレイヤーを向ける。
        // 戦闘側の既存 magnetism は currentTarget を別途参照する。
        if (!Entity::IsNull(lock.currentTarget)) {
            if (auto* targetTr = registry.GetComponent<TransformComponent>(lock.currentTarget)) {
                const float dx = targetTr->worldPosition.x - playerTr.worldPosition.x;
                const float dz = targetTr->worldPosition.z - playerTr.worldPosition.z;
                if (dx * dx + dz * dz > 0.0001f) {
                    if (auto* loco = registry.GetComponent<LocomotionStateComponent>(playerEntity)) {
                        loco->targetAngleY = std::atan2(dx, dz);
                    }
                }
            }
        }
    });
}
