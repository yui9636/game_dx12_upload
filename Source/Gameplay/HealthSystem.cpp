#include "HealthSystem.h"
#include "HealthComponent.h"
#include "DamageEventComponent.h"
#include "HitStopComponent.h"
#include "StateMachineParamsComponent.h"
#include "CharacterPhysicsComponent.h"
#include "Audio/AudioWorldSystem.h"
#include "Component/TransformComponent.h"
#include "EffectRuntime/EffectService.h"
#include "Engine/EngineKernel.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include "UI/DamageTextManager.h"

#include <algorithm>

namespace
{
    // 被弾後の標準無敵時間。reactionKind が分岐したら event ごとに調整できるようにする。
    constexpr float kInvincibleAfterHitSec = 0.5f;
    // 大きい値を優先する。既に走っているヒットストップを短くしない。
    constexpr float kVictimHitStopScale    = 0.05f;

    void TickInvincibility(Registry& registry, float dt)
    {
        Signature sig = CreateSignature<HealthComponent>();
        for (auto* arch : registry.GetAllArchetypes()) {
            if (!SignatureMatches(arch->GetSignature(), sig)) continue;
            auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<HealthComponent>());
            if (!col) continue;
            for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                auto& h = *static_cast<HealthComponent*>(col->Get(i));
                if (h.invincibleTimer > 0.0f) {
                    h.invincibleTimer -= dt;
                    if (h.invincibleTimer <= 0.0f) {
                        h.invincibleTimer = 0.0f;
                        h.isInvincible = false;
                    }
                }
                h.isDead = (h.health <= 0);
            }
        }
    }

    void ApplyDamageEvents(Registry& registry)
    {
        const auto& events = DamageEventRuntimeQueue::GetAll();
        for (const auto& ev : events) {
            HealthComponent* vh = registry.GetComponent<HealthComponent>(ev.victim);
            if (!vh) continue;
            if (vh->isDead || vh->isInvincible) continue;

            vh->health -= ev.amount;
            if (vh->health < 0) vh->health = 0;
            vh->lastDamage = ev.amount;
            vh->invincibleTimer = kInvincibleAfterHitSec;
            vh->isInvincible = true;
            vh->isDead = (vh->health <= 0);

            // StateMachine の Damaged 遷移を発火する。
            if (auto* smp = registry.GetComponent<StateMachineParamsComponent>(ev.victim)) {
                smp->SetParam("Damaged", 1.0f);
            }

            // 被弾側へヒットストップを伝播する。既に走っている停止時間は短くしない。
            if (auto* hs = registry.GetComponent<HitStopComponent>(ev.victim)) {
                if (ev.hitStopSec > hs->timer) {
                    hs->timer      = ev.hitStopSec;
                    hs->speedScale = kVictimHitStopScale;
                }
            }

            // 任意のノックバック。
            if (ev.knockbackPower > 0.0f) {
                if (auto* phys = registry.GetComponent<CharacterPhysicsComponent>(ev.victim)) {
                    phys->velocity.x = ev.knockbackDir.x * ev.knockbackPower;
                    phys->velocity.z = ev.knockbackDir.z * ev.knockbackPower;
                }
            }

            // 浮遊ダメージ数値。プール未初期化なら安全に何もしない。
            DamageTextManager::Instance().Spawn(ev.hitPoint, ev.amount);

            // 被弾 VFX。Hitbox item にパスが指定されていれば、
            // ワールド空間の接触点へ生成する。
            if (!ev.hitVfxPath.empty()) {
                EffectPlayDesc desc;
                desc.assetPath = ev.hitVfxPath;
                desc.position  = ev.hitPoint;
                desc.rotation  = { 0.0f, 0.0f, 0.0f, 1.0f };
                desc.scale     = { 1.0f, 1.0f, 1.0f };
                desc.loop      = false;
                desc.debugName = "Hit VFX";
                EffectService::Instance().PlayWorld(registry, desc);
            }

            // 被弾 SE。距離減衰が効くよう、常に 3D 位置付きで鳴らす。
            if (!ev.hitSfxPath.empty()) {
                auto& audio = EngineKernel::Instance().GetAudioWorld();
                audio.PlayTransient3D(ev.hitSfxPath, ev.hitPoint, 1.0f, 1.0f, false);
            }
        }

        DamageEventRuntimeQueue::Clear();
    }
}

void HealthSystem::Update(Registry& registry, float dt)
{
    TickInvincibility(registry, dt);
    ApplyDamageEvents(registry);
}
