#pragma once

class Registry;
class InputEventQueue;

class InputResolveSystem {
public:
    static void Update(Registry& registry, const InputEventQueue& queue, float dt);
};
