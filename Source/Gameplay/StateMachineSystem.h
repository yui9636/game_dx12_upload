#pragma once

class Registry;
// StateMachineAsset の遷移を毎フレーム評価する。
// ActionSystem / DodgeSystem に直書きされていた状態遷移を置き換える。
class StateMachineSystem
{
public:
    static void Update(Registry& registry, float dt);
    static void InvalidateAssetCache(const char* path = nullptr);
};
