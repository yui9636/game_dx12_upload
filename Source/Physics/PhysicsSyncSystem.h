#pragma once
#include "Registry/Registry.h"

// ECS の TransformComponent と Jolt の Body の位置・回転を同期するシステム。
class PhysicsSyncSystem {
public:
    // シミュレーション中かどうかに応じて、Transform から Body へ、または Body から Transform へ同期する。
    void Update(Registry& registry, bool isSimulation);
};
