#pragma once

class Registry;
class IInputBackend;

class InputFeedbackSystem {
public:
    static void Update(Registry& registry, IInputBackend& backend, float dt);
};
