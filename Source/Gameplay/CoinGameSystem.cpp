#include "CoinGameSystem.h"

#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "GameLoop/FlowEventQueue.h"
#include "Gameplay/CoinHUDTagComponent.h"
#include "Gameplay/CoinTagComponent.h"
#include "Gameplay/PlayerTagComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Entity/Entity.h"

#include <DirectXMath.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    struct CoinGameRuntime
    {
        bool  active         = false;
        int   totalCoins     = 10;
        int   collectedCoins = 0;
        float remainingTime  = 600.0f;
        bool  resultEmitted  = false;
    };

    CoinGameRuntime   g_state;
    CoinGameResult    g_lastResult = CoinGameResult::None;
    std::vector<FlowEvent> g_pendingEvents;

    constexpr float kCollectRadius = 2.0f;

    float Distance(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    void UpdateHUD(Registry& registry)
    {
        const int   mins  = static_cast<int>(g_state.remainingTime) / 60;
        const int   secs  = static_cast<int>(g_state.remainingTime) % 60;
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);

        char coinBuf[32];
        std::snprintf(coinBuf, sizeof(coinBuf), "%d / %d", g_state.collectedCoins, g_state.totalCoins);

        const char* resultStr =
            (g_lastResult == CoinGameResult::Clear) ? "GAME CLEAR!" :
            (g_lastResult == CoinGameResult::Over)  ? "GAME OVER"   : "";

        Query<CoinHUDTagComponent, TextComponent> q(registry);
        q.ForEach([&](CoinHUDTagComponent& tag, TextComponent& text) {
            switch (tag.tag) {
            case CoinHUDTag::CoinCount:  text.text = coinBuf;   break;
            case CoinHUDTag::Timer:      text.text = timeBuf;   break;
            case CoinHUDTag::ResultText: text.text = resultStr; break;
            }
        });
    }
}

void CoinGameSystem::Start(int totalCoins, float timeLimitSeconds)
{
    g_state               = CoinGameRuntime{};
    g_state.active        = true;
    g_state.totalCoins    = totalCoins;
    g_state.remainingTime = timeLimitSeconds;
    g_pendingEvents.clear();
}

void CoinGameSystem::Reset()
{
    g_state      = CoinGameRuntime{};
    g_lastResult = CoinGameResult::None;
    g_pendingEvents.clear();
}

void CoinGameSystem::DrainEvents(FlowEventQueue& outEvents)
{
    for (const FlowEvent& e : g_pendingEvents)
        outEvents.Push(e.name, e.value);
    g_pendingEvents.clear();
}

float         CoinGameSystem::GetRemainingTime() { return g_state.remainingTime; }
int           CoinGameSystem::GetCoinCount()     { return g_state.collectedCoins; }
int           CoinGameSystem::GetTotalCoins()    { return g_state.totalCoins; }
bool          CoinGameSystem::IsActive()         { return g_state.active; }
CoinGameResult CoinGameSystem::GetLastResult()   { return g_lastResult; }

void CoinGameSystem::Update(Registry& registry, float dt)
{
    // Auto-start when the scene has coin entities.
    if (!g_state.active && !g_state.resultEmitted) {
        int coinCount = 0;
        Query<CoinTagComponent> q(registry);
        q.ForEach([&](CoinTagComponent&) { ++coinCount; });
        if (coinCount > 0) Start(coinCount, 600.0f);
    }

    if (!g_state.active && !g_state.resultEmitted) {
        UpdateHUD(registry);
        return;
    }
    if (g_state.resultEmitted) {
        UpdateHUD(registry);
        return;
    }

    // Locate player world position.
    DirectX::XMFLOAT3 playerPos{ 0.0f, 0.0f, 0.0f };
    bool playerFound = false;
    {
        Query<PlayerTagComponent, TransformComponent> q(registry);
        q.ForEach([&](PlayerTagComponent&, TransformComponent& t) {
            if (!playerFound) { playerPos = t.worldPosition; playerFound = true; }
        });
    }

    if (!playerFound) { UpdateHUD(registry); return; }

    // Collect nearby coins.
    std::vector<EntityID> toDestroy;
    {
        Query<CoinTagComponent, TransformComponent> q(registry);
        q.ForEachWithEntity([&](EntityID e, CoinTagComponent& coin, TransformComponent& t) {
            if (coin.collected) return;
            if (Distance(playerPos, t.worldPosition) <= kCollectRadius) {
                coin.collected = true;
                g_state.collectedCoins++;
                toDestroy.push_back(e);
            }
        });
    }
    for (EntityID e : toDestroy) registry.DestroyEntity(e);

    // Win condition.
    if (g_state.collectedCoins >= g_state.totalCoins) {
        g_state.active        = false;
        g_state.resultEmitted = true;
        g_lastResult          = CoinGameResult::Clear;
        g_pendingEvents.push_back({ "GameClear", "" });
        UpdateHUD(registry);
        return;
    }

    // Timer countdown.
    g_state.remainingTime -= dt;
    if (g_state.remainingTime <= 0.0f) {
        g_state.remainingTime = 0.0f;
        g_state.active        = false;
        g_state.resultEmitted = true;
        g_lastResult          = CoinGameResult::Over;
        g_pendingEvents.push_back({ "GameOver", "" });
    }

    UpdateHUD(registry);
}
