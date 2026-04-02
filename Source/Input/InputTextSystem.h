#pragma once

class Registry;
class InputEventQueue;
class IInputBackend;

class InputTextSystem {
public:
    static void Update(Registry& registry, const InputEventQueue& queue, IInputBackend& backend);
};
