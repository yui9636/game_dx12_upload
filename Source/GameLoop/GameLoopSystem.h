// GameLoopSystem のシステム宣言をまとめます。
#pragma once

class FlowEventQueue;
class Registry;
struct GameLoopAsset;
struct GameLoopRuntime;

class GameLoopSystem
{
public:
    static void Update(
        const GameLoopAsset& asset,
        GameLoopRuntime&     runtime,
        Registry&            gameRegistry,
        Registry&            gameLoopRegistry,
        FlowEventQueue&      flowEvents,
        float                dt);
};
