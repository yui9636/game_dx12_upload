#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

enum class ActionTriggerType : uint8_t {
    Pressed, Released, Held, DoubleTap
};

struct ActionBinding {
    std::string actionName;
    uint32_t scancode = 0;       // SDL scancode
    uint8_t mouseButton = 0;
    uint8_t gamepadButton = 0xFF; // 0xFF = unbound
    uint8_t gamepadAxis = 0xFF;
    float axisDirection = 1.0f;   // +1 or -1
    ActionTriggerType trigger = ActionTriggerType::Pressed;
};

struct AxisBinding {
    std::string axisName;
    uint32_t positiveKey = 0;
    uint32_t negativeKey = 0;
    uint8_t gamepadAxis = 0xFF;
    float deadzone = 0.15f;
    float sensitivity = 1.0f;
};

struct InputActionMapAsset {
    std::string name;
    std::string contextCategory;
    std::vector<ActionBinding> actions;
    std::vector<AxisBinding> axes;
    int holdThresholdFrames = 30;
    int doubleTapGapFrames = 15;

    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

    // Cache
    static InputActionMapAsset* Get(const std::string& path);
    static void ClearCache();

private:
    static std::unordered_map<std::string, InputActionMapAsset> s_cache;
};
