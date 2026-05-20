#include "ProjectileSystem.h"

#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Component/MeshComponent.h"
#include "Component/ProjectileComponent.h"
#include "Component/ProjectileEmitterComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Gameplay/DamageEventComponent.h"

#include <DirectXMath.h>
#include <algorithm>
#include <vector>

using namespace DirectX;

namespace {
    constexpr float kTargetBodyRadius = 0.9f; // approximate fighter body radius

    struct TargetInfo {
        EntityID entity;
        XMFLOAT3 position;
    };

    void CollectTargets(Registry& reg, bool playerFaction, std::vector<TargetInfo>& out)
    {
        if (playerFaction) {
            Query<PlayerTagComponent, HealthComponent, TransformComponent> q(reg);
            q.ForEachWithEntity([&](EntityID e, PlayerTagComponent&, HealthComponent& h, TransformComponent& t) {
                if (!h.isDead) out.push_back({ e, t.worldPosition });
            });
        } else {
            Query<EnemyTagComponent, HealthComponent, TransformComponent> q(reg);
            q.ForEachWithEntity([&](EntityID e, EnemyTagComponent&, HealthComponent& h, TransformComponent& t) {
                if (!h.isDead) out.push_back({ e, t.worldPosition });
            });
        }
    }

    bool FindAnyTargetPos(Registry& reg, bool playerFaction, XMFLOAT3& out)
    {
        std::vector<TargetInfo> targets;
        CollectTargets(reg, playerFaction, targets);
        if (targets.empty()) return false;
        out = targets.front().position;
        return true;
    }

    XMVECTOR RotateAroundY(const XMVECTOR& dir, float radians)
    {
        return XMVector3Rotate(dir, XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), radians));
    }

    // A volley gathered during an emitter pass; spawned afterwards so the
    // registry is not modified while a query is iterating.
    struct VolleyRequest {
        EntityID owner = 0;
        ProjectileVolleyParams params;
        XMFLOAT3 muzzle = { 0.0f, 0.0f, 0.0f };
    };
}

void ProjectileSystem::SpawnVolley(Registry& registry, EntityID owner,
                                   const XMFLOAT3& muzzle,
                                   const ProjectileVolleyParams& params)
{
    // Horizontal aim toward the target faction (or +X if no target exists).
    XMFLOAT3 targetPos;
    XMVECTOR aim;
    if (FindAnyTargetPos(registry, params.targetsPlayer, targetPos)) {
        aim = XMVectorSubtract(XMLoadFloat3(&targetPos), XMLoadFloat3(&muzzle));
    } else {
        aim = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    aim = XMVectorSetY(aim, 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(aim)) < 1.0e-6f) {
        aim = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    aim = XMVector3Normalize(aim);

    auto spawnBullet = [&](const XMVECTOR& dir) {
        const EntityID bullet = registry.CreateEntity();

        TransformComponent t;
        t.localPosition = muzzle;
        t.localScale = { params.bulletScale, params.bulletScale, params.bulletScale };
        t.worldPosition = muzzle;
        t.worldScale = t.localScale;
        const XMMATRIX world = XMMatrixAffineTransformation(
            XMLoadFloat3(&t.localScale),
            XMVectorZero(),
            XMLoadFloat4(&t.localRotation),
            XMLoadFloat3(&t.localPosition));
        XMStoreFloat4x4(&t.localMatrix, world);
        XMStoreFloat4x4(&t.worldMatrix, world);
        t.prevWorldMatrix = t.worldMatrix;
        t.isDirty = true;
        registry.AddComponent(bullet, t);

        ProjectileComponent p;
        XMFLOAT3 d;
        XMStoreFloat3(&d, dir);
        p.velocity = { d.x * params.bulletSpeed, d.y * params.bulletSpeed, d.z * params.bulletSpeed };
        p.lifetime = params.bulletLifetime;
        p.damage = params.bulletDamage;
        p.radius = params.bulletRadius;
        p.owner = owner;
        p.targetsPlayer = params.targetsPlayer;
        registry.AddComponent(bullet, p);

        if (!params.bulletModelPath.empty()) {
            MeshComponent m;
            m.modelFilePath = params.bulletModelPath;
            m.isVisible = true;
            m.castShadow = false;
            registry.AddComponent(bullet, m);
        }
    };

    if (params.pattern == 2) {
        // Ring: evenly around 360 degrees.
        const int n = (std::max)(params.bulletsPerVolley, 1);
        for (int i = 0; i < n; ++i) {
            const float angle = XM_2PI * (static_cast<float>(i) / static_cast<float>(n));
            spawnBullet(RotateAroundY(aim, angle));
        }
    } else if (params.pattern == 1) {
        // Spread: fan centered on the aim direction.
        const int n = (std::max)(params.bulletsPerVolley, 1);
        const float span = XMConvertToRadians(params.spreadAngleDeg);
        for (int i = 0; i < n; ++i) {
            const float t = (n > 1) ? (static_cast<float>(i) / static_cast<float>(n - 1)) : 0.5f;
            spawnBullet(RotateAroundY(aim, -span * 0.5f + span * t));
        }
    } else {
        // Aimed: single bullet straight at the target.
        spawnBullet(aim);
    }
}

void ProjectileSystem::Update(Registry& registry, float dt)
{
    if (dt <= 0.0f) {
        return; // paused / not in play
    }

    // ---- Pass 1: tick emitters, gather volleys ----
    std::vector<VolleyRequest> volleys;
    {
        Query<ProjectileEmitterComponent, TransformComponent> q(registry);
        q.ForEachWithEntity([&](EntityID owner, ProjectileEmitterComponent& em, TransformComponent& tr) {
            if (!em.active) {
                return;
            }
            em.fireTimer -= dt;
            if (em.fireTimer > 0.0f) {
                return;
            }
            em.fireTimer = (std::max)(em.fireInterval, 0.05f);

            const XMVECTOR rot = XMLoadFloat4(&tr.worldRotation);
            const XMVECTOR offset = XMVector3Rotate(XMLoadFloat3(&em.muzzleOffset), rot);

            VolleyRequest req;
            req.owner = owner;
            XMStoreFloat3(&req.muzzle, XMVectorAdd(XMLoadFloat3(&tr.worldPosition), offset));
            req.params.pattern = em.pattern;
            req.params.bulletsPerVolley = em.bulletsPerVolley;
            req.params.spreadAngleDeg = em.spreadAngleDeg;
            req.params.bulletSpeed = em.bulletSpeed;
            req.params.bulletLifetime = em.bulletLifetime;
            req.params.bulletDamage = em.bulletDamage;
            req.params.bulletRadius = em.bulletRadius;
            req.params.bulletScale = em.bulletScale;
            req.params.targetsPlayer = em.targetsPlayer;
            req.params.bulletModelPath = em.bulletModelPath;
            volleys.push_back(std::move(req));
        });
    }
    for (const VolleyRequest& v : volleys) {
        SpawnVolley(registry, v.owner, v.muzzle, v.params);
    }

    // ---- Pass 2: gather collision targets ----
    std::vector<TargetInfo> playerTargets;
    std::vector<TargetInfo> enemyTargets;
    CollectTargets(registry, true, playerTargets);
    CollectTargets(registry, false, enemyTargets);

    // ---- Pass 3: move bullets, resolve collisions ----
    std::vector<EntityID> toDestroy;
    {
        Query<ProjectileComponent, TransformComponent> q(registry);
        q.ForEachWithEntity([&](EntityID bullet, ProjectileComponent& p, TransformComponent& tr) {
            tr.localPosition.x += p.velocity.x * dt;
            tr.localPosition.y += p.velocity.y * dt;
            tr.localPosition.z += p.velocity.z * dt;
            tr.isDirty = true;

            p.lifetime -= dt;
            if (p.lifetime <= 0.0f) {
                toDestroy.push_back(bullet);
                return;
            }

            const std::vector<TargetInfo>& targets = p.targetsPlayer ? playerTargets : enemyTargets;
            const float hitDist = p.radius + kTargetBodyRadius;
            const float hitDistSq = hitDist * hitDist;

            for (const TargetInfo& target : targets) {
                const float dx = target.position.x - tr.localPosition.x;
                const float dy = target.position.y - tr.localPosition.y;
                const float dz = target.position.z - tr.localPosition.z;
                if (dx * dx + dy * dy + dz * dz <= hitDistSq) {
                    DamageEventComponent::Event ev;
                    ev.attacker = p.owner;
                    ev.victim = target.entity;
                    ev.amount = p.damage;
                    ev.hitPoint = tr.localPosition;
                    DamageEventRuntimeQueue::Push(ev);
                    toDestroy.push_back(bullet);
                    break;
                }
            }
        });
    }

    for (const EntityID e : toDestroy) {
        if (registry.IsAlive(e)) {
            registry.DestroyEntity(e);
        }
    }
}
