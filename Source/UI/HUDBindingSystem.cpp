#include "HUDBindingSystem.h"

#include "Component/TransformComponent.h"
#include "Gameplay/HUDLinkComponent.h"
#include "Gameplay/HealthComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"

namespace
{
    HUDBindingSystem::State g_state;

    float SafeRatio(int current, int maxValue)
    {
        if (maxValue <= 0) return 0.0f;
        if (current <= 0) return 0.0f;
        const float r = static_cast<float>(current) / static_cast<float>(maxValue);
        return r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
    }
}

void HUDBindingSystem::Update(Registry& registry)
{
    g_state = State{};

    Query<HealthComponent, HUDLinkComponent> q(registry);
    q.ForEachWithEntity([&](EntityID e, HealthComponent& h, HUDLinkComponent& link) {
        const float ratio = SafeRatio(h.health, h.maxHealth);

        if (link.asPlayerHUD && !g_state.playerActive) {
            g_state.playerActive  = true;
            g_state.playerEntity  = e;
            g_state.playerRatio   = ratio;
            g_state.playerHP      = h.health;
            g_state.playerMaxHP   = h.maxHealth;
        }
        if (link.asBossHUD && !g_state.bossActive) {
            g_state.bossActive   = true;
            g_state.bossEntity   = e;
            g_state.bossRatio    = ratio;
            g_state.bossHP       = h.health;
            g_state.bossMaxHP    = h.maxHealth;
        }
        if (link.asWorldFloat) {
            WorldEntry entry;
            entry.entity = e;
            entry.ratio  = ratio;
            entry.hp     = h.health;
            entry.maxHP  = h.maxHealth;
            entry.worldOffsetY = link.worldOffsetY;
            if (auto* tr = registry.GetComponent<TransformComponent>(e)) {
                entry.worldPos = tr->worldPosition;
                entry.worldPos.y += link.worldOffsetY;
            }
            g_state.world.push_back(entry);
        }
    });
}

const HUDBindingSystem::State& HUDBindingSystem::GetState()
{
    return g_state;
}
