#pragma once
#include <vector>
#include <cstdint>
#include <mutex>
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

    // Recent ring buffer 各 entry の monotonic sequence (overflow 後も cursor 比較が安全)。
    struct RecentEntry
    {
        Event event;
        uint64_t sequence = 0;
    };

    static void Push(const Event& event)
    {
        std::lock_guard<std::recursive_mutex> lock(Mutex());
        Events().push_back(event);
        const uint64_t seq = ++NextSequence();
        RecentEvents().push_back({ event, seq });
        RecentRaw().push_back(event); // 旧 API 互換用
        if (RecentEvents().size() > 128) {
            RecentEvents().erase(RecentEvents().begin(), RecentEvents().begin() + (RecentEvents().size() - 128));
        }
        if (RecentRaw().size() > 128) {
            RecentRaw().erase(RecentRaw().begin(), RecentRaw().begin() + (RecentRaw().size() - 128));
        }
    }

    // 旧 API: 後方互換用に raw 参照を返す。マルチスレッド環境では SnapshotAll() を使うこと。
    static const std::vector<Event>& GetAll()
    {
        return Events();
    }

    // 旧 API: 後方互換のため sequence なしの Event のみを返す (非 thread-safe)。
    static const std::vector<Event>& GetRecent()
    {
        return RecentRaw();
    }

    // 旧 API: sequence 付きの recent entries を返す (非 thread-safe; cursor pull の race を避けるため Snapshot 推奨)。
    static const std::vector<RecentEntry>& GetRecentWithSeq()
    {
        return RecentEvents();
    }

    // Thread-safe snapshot 系。pull cursor 系 handler から使う。
    static std::vector<Event> SnapshotAll()
    {
        std::lock_guard<std::recursive_mutex> lock(Mutex());
        return Events();
    }

    static std::vector<RecentEntry> SnapshotRecentWithSeq()
    {
        std::lock_guard<std::recursive_mutex> lock(Mutex());
        return RecentEvents();
    }

    static uint64_t GetLatestSequence()
    {
        std::lock_guard<std::recursive_mutex> lock(Mutex());
        return NextSequence();
    }

    static void Clear()
    {
        std::lock_guard<std::recursive_mutex> lock(Mutex());
        Events().clear();
    }

    static void ClearRecent()
    {
        std::lock_guard<std::recursive_mutex> lock(Mutex());
        RecentEvents().clear();
        RecentRaw().clear();
    }

private:
    static std::vector<Event>& Events()
    {
        static std::vector<Event> events;
        return events;
    }

    static std::vector<RecentEntry>& RecentEvents()
    {
        static std::vector<RecentEntry> events;
        return events;
    }

    static std::vector<Event>& RecentRaw()
    {
        static std::vector<Event> events;
        return events;
    }

    static uint64_t& NextSequence()
    {
        static uint64_t seq = 0;
        return seq;
    }

    // Push / Snapshot を保護する mutex。recursive 版なので、
    // Snapshot callback 中に同じ thread から Push しても deadlock しない (将来の安全性)。
    static std::recursive_mutex& Mutex()
    {
        static std::recursive_mutex m;
        return m;
    }
};
