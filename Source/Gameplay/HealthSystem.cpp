#include "HealthSystem.h"
#include "HealthComponent.h"
#include "DamageEventComponent.h"
#include "HitStopComponent.h"
#include "StateMachineParamsComponent.h"
#include "CharacterPhysicsComponent.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include "System/Query.h"
#include "UI/DamageTextManager.h"

#include <algorithm>

namespace
{
    // Default i-frames after taking a hit. Tunable per-event later if reactionKind diverges.
    constexpr float kInvincibleAfterHitSec = 0.5f;
    // Stack-the-larger semantics: don't shorten an already-running hitstop.
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
        DamageEventComponent* queue = nullptr;
        Query<DamageEventComponent> q(registry);
        q.ForEach([&](DamageEventComponent& deq) {
            if (!queue) queue = &deq;
        });
        if (!queue) return;

        for (const auto& ev : queue->events) {
            HealthComponent* vh = registry.GetComponent<HealthComponent>(ev.victim);
            if (!vh) continue;
            if (vh->isDead || vh->isInvincible) continue;

            vh->health -= ev.amount;
            if (vh->health < 0) vh->health = 0;
            vh->lastDamage = ev.amount;
            vh->invincibleTimer = kInvincibleAfterHitSec;
            vh->isInvincible = true;
            vh->isDead = (vh->health <= 0);

            // Trigger the StateMachine Damaged transition.
            if (auto* smp = registry.GetComponent<StateMachineParamsComponent>(ev.victim)) {
                smp->SetParam("Damaged", 1.0f);
            }

            // Propagate hitstop to the victim. Don't shorten an already running freeze.
            if (auto* hs = registry.GetComponent<HitStopComponent>(ev.victim)) {
                if (ev.hitStopSec > hs->timer) {
                    hs->timer      = ev.hitStopSec;
                    hs->speedScale = kVictimHitStopScale;
                }
            }

            // Optional knockback.
            if (ev.knockbackPower > 0.0f) {
                if (auto* phys = registry.GetComponent<CharacterPhysicsComponent>(ev.victim)) {
                    phys->velocity.x = ev.knockbackDir.x * ev.knockbackPower;
                    phys->velocity.z = ev.knockbackDir.z * ev.knockbackPower;
                }
            }

            // Floating damage number. Safe no-op if the pool wasn't initialized.
            DamageTextManager::Instance().Spawn(ev.hitPoint, ev.amount);
        }

        queue->events.clear();
    }
}

void HealthSystem::Update(Registry& registry, float dt)
{
    TickInvincibility(registry, dt);
    ApplyDamageEvents(registry);
}
