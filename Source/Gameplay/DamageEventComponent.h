#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <DirectXMath.h>

#include "Entity/Entity.h"

// Legacy serialized component. Runtime damage events are now stored in
// DamageEventRuntimeQueue so no "_DamageEventQueue" entity is required.
struct DamageEventComponent {
    struct Event {
        EntityID attacker = Entity::NULL_ID;
        EntityID victim   = Entity::NULL_ID;
        int      amount   = 0;
        DirectX::XMFLOAT3 hitPoint{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 knockbackDir{ 0.0f, 0.0f, 0.0f };
        float    knockbackPower = 0.0f;
        float    hitStopSec     = 0.08f;
        uint8_t  reactionKind   = 0; // 0=light, 1=heavy, 2=launch (v1 = light only)

        // Snapshot of the active Hitbox item's hit feedback paths (resolved
        // by DamageSystem at the moment the event is created). HealthSystem
        // uses these to spawn VFX / SE at hitPoint. Empty = silent / no VFX.
        std::string hitVfxPath;
        std::string hitSfxPath;
    };
    std::vector<Event> events;
};

class DamageEventRuntimeQueue
{
public:
    using Event = DamageEventComponent::Event;

    static void Push(const Event& event)
    {
        Events().push_back(event);
    }

    static const std::vector<Event>& GetAll()
    {
        return Events();
    }

    static void Clear()
    {
        Events().clear();
    }

private:
    static std::vector<Event>& Events()
    {
        static std::vector<Event> events;
        return events;
    }
};
