#pragma once
#include <cstdint>

class FlowEventQueue;
class Registry;

enum class CoinGameResult : uint8_t { None = 0, Clear = 1, Over = 2 };

class CoinGameSystem {
public:
    static void Update(Registry& registry, float dt);
    static void Start(int totalCoins = 10, float timeLimitSeconds = 600.0f);
    static void Reset();
    static void DrainEvents(FlowEventQueue& outEvents);

    static float          GetRemainingTime();
    static int            GetCoinCount();
    static int            GetTotalCoins();
    static bool           IsActive();
    static CoinGameResult GetLastResult();
};
