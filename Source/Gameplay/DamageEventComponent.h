#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <DirectXMath.h>

#include "Entity/Entity.h"

// 旧シリアライズ用 component。実行時の damage event は現在、
// DamageEventRuntimeQueue に保存されるため "_DamageEventQueue" entity は不要。
struct DamageEventComponent {
    struct Event {
        EntityID attacker = Entity::NULL_ID;
        EntityID victim   = Entity::NULL_ID;
        int      amount   = 0;
        DirectX::XMFLOAT3 hitPoint{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 knockbackDir{ 0.0f, 0.0f, 0.0f };
        float    knockbackPower = 0.0f;
        float    hitStopSec     = 0.08f;
        uint8_t  reactionKind   = 0; // 0=軽量、1=重量、2=打ち上げ（v1 は light のみ）

        // active Hitbox item の被弾演出パスのスナップショット。
        // event 作成時に DamageSystem が解決し、HealthSystem が
        // hitPoint に VFX / SE を出すために使う。空なら無音・VFX なし。
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
        RecentEvents().push_back(event);
        if (RecentEvents().size() > 128) {
            RecentEvents().erase(RecentEvents().begin(), RecentEvents().begin() + (RecentEvents().size() - 128));
        }
    }

    static const std::vector<Event>& GetAll()
    {
        return Events();
    }

    static const std::vector<Event>& GetRecent()
    {
        return RecentEvents();
    }

    static void Clear()
    {
        Events().clear();
    }

    static void ClearRecent()
    {
        RecentEvents().clear();
    }

private:
    static std::vector<Event>& Events()
    {
        static std::vector<Event> events;
        return events;
    }

    static std::vector<Event>& RecentEvents()
    {
        static std::vector<Event> events;
        return events;
    }
};
