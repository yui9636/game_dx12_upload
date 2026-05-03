#include "DamageSystem.h"

#include "Collision/Collision.h"
#include "Collision/CollisionManager.h"
#include "Component/TransformComponent.h"
#include "Gameplay/ActionDatabaseComponent.h"
#include "Gameplay/ActionStateComponent.h"
#include "Gameplay/DamageEventComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/HitboxTrackingComponent.h"
#include "Gameplay/TeamComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"

#include <DirectXMath.h>
#include <cmath>
#include <cstdint>

namespace
{
    EntityID UserPtrToEntity(void* userPtr)
    {
        return static_cast<EntityID>(reinterpret_cast<uintptr_t>(userPtr));
    }

    DamageEventComponent* FindDamageEventQueue(Registry& registry)
    {
        DamageEventComponent* found = nullptr;
        Query<DamageEventComponent> q(registry);
        q.ForEach([&](DamageEventComponent& deq) {
            if (!found) found = &deq;
        });
        return found;
    }

    bool AlreadyHit(const HitboxTrackingComponent& track, EntityID victim)
    {
        for (uint8_t i = 0; i < track.hitEntityCount; ++i) {
            if (track.hitEntities[i] == victim) return true;
        }
        return false;
    }

    void RecordHit(HitboxTrackingComponent& track, EntityID victim)
    {
        constexpr uint8_t kMax = static_cast<uint8_t>(
            sizeof(track.hitEntities) / sizeof(track.hitEntities[0]));
        if (track.hitEntityCount < kMax) {
            track.hitEntities[track.hitEntityCount++] = victim;
        }
    }

    DirectX::XMFLOAT3 HorizontalDirection(const DirectX::XMFLOAT3& from,
                                          const DirectX::XMFLOAT3& to)
    {
        DirectX::XMFLOAT3 dir{ to.x - from.x, 0.0f, to.z - from.z };
        const float lenSq = dir.x * dir.x + dir.z * dir.z;
        if (lenSq <= 1e-6f) {
            return { 0.0f, 0.0f, 0.0f };
        }
        const float invLen = 1.0f / std::sqrt(lenSq);
        dir.x *= invLen;
        dir.z *= invLen;
        return dir;
    }

    int ResolveAttackDamage(Registry& registry, EntityID attacker)
    {
        const ActionStateComponent* as = registry.GetComponent<ActionStateComponent>(attacker);
        const ActionDatabaseComponent* ad = registry.GetComponent<ActionDatabaseComponent>(attacker);
        if (!as || !ad) return 1;

        const int idx = as->currentNodeIndex;
        if (idx < 0 || idx >= static_cast<int>(ad->nodeCount)) return 1;
        const int dmg = ad->nodes[idx].damageVal;
        return dmg > 0 ? dmg : 1;
    }
}

void DamageSystem::Update(Registry& registry)
{
    DamageEventComponent* queue = FindDamageEventQueue(registry);
    if (!queue) return;

    auto& cm = CollisionManager::Instance();
    std::vector<CollisionContact> contacts;
    cm.ComputeAllContacts(contacts);
    if (contacts.empty()) return;

    for (const auto& contact : contacts) {
        const Collider* a = cm.Get(contact.idA);
        const Collider* b = cm.Get(contact.idB);
        if (!a || !b) continue;
        if (!a->enabled || !b->enabled) continue;

        // Need exactly one Attack and one Body.
        const Collider* attackCol = nullptr;
        const Collider* bodyCol   = nullptr;
        if (a->attribute == ColliderAttribute::Attack && b->attribute == ColliderAttribute::Body) {
            attackCol = a; bodyCol = b;
        } else if (a->attribute == ColliderAttribute::Body && b->attribute == ColliderAttribute::Attack) {
            attackCol = b; bodyCol = a;
        } else {
            continue;
        }

        const EntityID attacker = UserPtrToEntity(attackCol->userPtr);
        const EntityID victim   = UserPtrToEntity(bodyCol->userPtr);
        if (Entity::IsNull(attacker) || Entity::IsNull(victim)) continue;
        if (attacker == victim) continue;

        // Friendly fire filter.
        const TeamComponent* atkTeam = registry.GetComponent<TeamComponent>(attacker);
        const TeamComponent* vicTeam = registry.GetComponent<TeamComponent>(victim);
        if (atkTeam && vicTeam && atkTeam->teamId == vicTeam->teamId) continue;

        // Multi-hit prevention (same swing).
        HitboxTrackingComponent* track = registry.GetComponent<HitboxTrackingComponent>(attacker);
        if (track && AlreadyHit(*track, victim)) continue;

        // Skip dead / invincible targets.
        const HealthComponent* vh = registry.GetComponent<HealthComponent>(victim);
        if (!vh || vh->isDead || vh->isInvincible || vh->health <= 0) continue;

        DamageEventComponent::Event ev{};
        ev.attacker = attacker;
        ev.victim   = victim;
        ev.amount   = ResolveAttackDamage(registry, attacker);
        ev.hitPoint = contact.hit.hitPosition;

        const TransformComponent* atkTr = registry.GetComponent<TransformComponent>(attacker);
        const TransformComponent* vicTr = registry.GetComponent<TransformComponent>(victim);
        if (atkTr && vicTr) {
            ev.knockbackDir = HorizontalDirection(atkTr->worldPosition, vicTr->worldPosition);
        }
        ev.knockbackPower = 0.0f; // v1: reaction-driven, no positional shove
        ev.hitStopSec     = 0.08f;
        ev.reactionKind   = 0;

        queue->events.push_back(ev);
        if (track) RecordHit(*track, victim);
    }
}
