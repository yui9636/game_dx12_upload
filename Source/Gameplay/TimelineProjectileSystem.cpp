#include "TimelineProjectileSystem.h"

#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Gameplay/ProjectileSystem.h"
#include "PlayerEditor/TimelineAsset.h"

#include <DirectXMath.h>
#include <vector>

using namespace DirectX;

namespace {
    // Runtime item type written by TimelineAssetRuntimeBuilder for Projectile tracks.
    constexpr int kProjectileItemType = static_cast<int>(TimelineTrackType::Projectile);

    struct FireRequest {
        EntityID owner = 0;
        XMFLOAT3 muzzle = { 0.0f, 0.0f, 0.0f };
        ProjectileVolleyParams params;
    };
}

void TimelineProjectileSystem::Update(Registry& registry, float dt)
{
    if (dt <= 0.0f) {
        return; // paused / not in play
    }

    std::vector<FireRequest> requests;
    {
        Query<TimelineComponent, TimelineItemBuffer, TransformComponent> q(registry);
        q.ForEachWithEntity([&](EntityID owner, TimelineComponent& tl, TimelineItemBuffer& buf, TransformComponent& tr) {
            for (auto& item : buf.items) {
                if (item.type != kProjectileItemType) {
                    continue;
                }
                const bool inWindow =
                    tl.currentFrame >= item.start && tl.currentFrame <= item.end;

                if (inWindow && !item.fired) {
                    item.fired = true;

                    const GEProjectilePayload& pl = item.proj;
                    const XMVECTOR rot = XMLoadFloat4(&tr.worldRotation);
                    const XMVECTOR off = XMVector3Rotate(XMLoadFloat3(&pl.offsetLocal), rot);

                    FireRequest req;
                    req.owner = owner;
                    XMStoreFloat3(&req.muzzle, XMVectorAdd(XMLoadFloat3(&tr.worldPosition), off));
                    req.params.pattern          = pl.pattern;
                    req.params.bulletsPerVolley = pl.bulletsPerVolley;
                    req.params.spreadAngleDeg   = pl.spreadAngleDeg;
                    req.params.bulletSpeed      = pl.bulletSpeed;
                    req.params.bulletLifetime   = pl.bulletLifetime;
                    req.params.bulletDamage     = pl.bulletDamage;
                    req.params.bulletRadius     = pl.bulletRadius;
                    req.params.bulletScale      = pl.bulletScale;
                    req.params.targetsPlayer    = pl.targetsPlayer;
                    req.params.bulletModelPath  = pl.bulletModelPath;
                    requests.push_back(std::move(req));
                }
                else if (!inWindow && item.fired) {
                    item.fired = false; // re-arm for looping animations
                }
            }
        });
    }

    for (const FireRequest& r : requests) {
        ProjectileSystem::SpawnVolley(registry, r.owner, r.muzzle, r.params);
    }
}
