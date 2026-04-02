#pragma once
class Registry;
struct EngineTime;
class LocomotionSystem {
public:
    static void Update(Registry& registry, float dt);
};
