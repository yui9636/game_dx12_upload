#include "CharacterPhysicsSystem.h"
#include "CharacterPhysicsComponent.h"
#include "ActionStateComponent.h"
#include "StageBoundsComponent.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cmath>
#include <algorithm>

void CharacterPhysicsSystem::Update(Registry& registry, float dt) {
    if (dt <= 0.0f) return;

    // ステージ半径を取得する（最初の StageBoundsComponent）。
    float stageRadius = 9999.0f;
    {
        Signature boundsSig = CreateSignature<StageBoundsComponent>();
        for (auto* arch : registry.GetAllArchetypes()) {
            if (!SignatureMatches(arch->GetSignature(), boundsSig)) continue;
            auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<StageBoundsComponent>());
            if (col && arch->GetEntityCount() > 0) {
                stageRadius = static_cast<StageBoundsComponent*>(col->Get(0))->radius;
                break;
            }
        }
    }

    Signature sig = CreateSignature<CharacterPhysicsComponent, TransformComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* physCol   = arch->GetColumn(TypeManager::GetComponentTypeID<CharacterPhysicsComponent>());
        auto* transCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        if (!physCol || !transCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& phys  = *static_cast<CharacterPhysicsComponent*>(physCol->Get(i));
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));

            // 死亡済みエンティティは動かさない。
            if (actionCol) {
                auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
                if (action.state == CharacterState::Dead) {
                    phys.velocity = { 0, 0, 0 };
                    phys.verticalVelocity = 0.0f;
                    continue;
                }
            }

            // 重力
            if (!phys.isGround) {
                phys.verticalVelocity += phys.gravity * dt;
            }

            // ロコモーション調整が崩れてもプレイヤーが瞬間移動しないよう、水平移動を制限する。
            if (phys.maxMoveSpeed > 0.0f) {
                const float speedSq = phys.velocity.x * phys.velocity.x + phys.velocity.z * phys.velocity.z;
                const float maxSpeedSq = phys.maxMoveSpeed * phys.maxMoveSpeed;
                if (speedSq > maxSpeedSq && speedSq > 0.0001f) {
                    const float speed = sqrtf(speedSq);
                    const float scale = phys.maxMoveSpeed / speed;
                    phys.velocity.x *= scale;
                    phys.velocity.z *= scale;
                }
            }

            // 位置を積分する。
            trans.localPosition.x += phys.velocity.x * dt;
            trans.localPosition.y += phys.verticalVelocity * dt;
            trans.localPosition.z += phys.velocity.z * dt;

            // 地面へ押し戻す（y=0 の平面地形）。
            if (trans.localPosition.y <= 0.0f) {
                trans.localPosition.y = 0.0f;
                phys.verticalVelocity = 0.0f;
                phys.isGround = true;
            }

            // 円形ステージ範囲内へ押し戻し、外向き速度を取り除く。
            if (stageRadius < 9000.0f) {
                float x = trans.localPosition.x;
                float z = trans.localPosition.z;
                float distSq = x * x + z * z;
                float limit = stageRadius;
                if (distSq > limit * limit) {
                    float dist = sqrtf(distSq);
                    if (dist > 0.0001f) {
                        float nx = x / dist;
                        float nz = z / dist;
                        trans.localPosition.x = nx * limit;
                        trans.localPosition.z = nz * limit;

                        // 外向きの速度成分を取り除く。
                        float outward = phys.velocity.x * nx + phys.velocity.z * nz;
                        if (outward > 0.0f) {
                            phys.velocity.x -= nx * outward;
                            phys.velocity.z -= nz * outward;
                        }
                    }
                }
            }
        }
    }
}
