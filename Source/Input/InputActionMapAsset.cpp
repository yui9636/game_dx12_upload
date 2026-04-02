#include "InputActionMapAsset.h"
#include "Console/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

std::unordered_map<std::string, InputActionMapAsset> InputActionMapAsset::s_cache;

bool InputActionMapAsset::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("[InputActionMapAsset] Failed to open: %s", path.c_str());
        return false;
    }

    try {
        json j;
        file >> j;

        name = j.value("name", "");
        contextCategory = j.value("contextCategory", "");
        holdThresholdFrames = j.value("holdThresholdFrames", 30);
        doubleTapGapFrames = j.value("doubleTapGapFrames", 15);

        actions.clear();
        if (j.contains("actions")) {
            for (auto& a : j["actions"]) {
                ActionBinding ab;
                ab.actionName = a.value("actionName", "");
                ab.scancode = a.value("scancode", 0u);
                ab.mouseButton = a.value("mouseButton", (uint8_t)0);
                ab.gamepadButton = a.value("gamepadButton", (uint8_t)0xFF);
                ab.gamepadAxis = a.value("gamepadAxis", (uint8_t)0xFF);
                ab.axisDirection = a.value("axisDirection", 1.0f);
                std::string trigStr = a.value("trigger", "Pressed");
                if (trigStr == "Released") ab.trigger = ActionTriggerType::Released;
                else if (trigStr == "Held") ab.trigger = ActionTriggerType::Held;
                else if (trigStr == "DoubleTap") ab.trigger = ActionTriggerType::DoubleTap;
                else ab.trigger = ActionTriggerType::Pressed;
                actions.push_back(ab);
            }
        }

        axes.clear();
        if (j.contains("axes")) {
            for (auto& ax : j["axes"]) {
                AxisBinding ab;
                ab.axisName = ax.value("axisName", "");
                ab.positiveKey = ax.value("positiveKey", 0u);
                ab.negativeKey = ax.value("negativeKey", 0u);
                ab.gamepadAxis = ax.value("gamepadAxis", (uint8_t)0xFF);
                ab.deadzone = ax.value("deadzone", 0.15f);
                ab.sensitivity = ax.value("sensitivity", 1.0f);
                axes.push_back(ab);
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("[InputActionMapAsset] Parse error in %s: %s", path.c_str(), e.what());
        return false;
    }

    return true;
}

bool InputActionMapAsset::SaveToFile(const std::string& path) const {
    json j;
    j["name"] = name;
    j["contextCategory"] = contextCategory;
    j["holdThresholdFrames"] = holdThresholdFrames;
    j["doubleTapGapFrames"] = doubleTapGapFrames;

    j["actions"] = json::array();
    for (auto& a : actions) {
        json aj;
        aj["actionName"] = a.actionName;
        aj["scancode"] = a.scancode;
        aj["mouseButton"] = a.mouseButton;
        aj["gamepadButton"] = a.gamepadButton;
        aj["gamepadAxis"] = a.gamepadAxis;
        aj["axisDirection"] = a.axisDirection;
        switch (a.trigger) {
        case ActionTriggerType::Released: aj["trigger"] = "Released"; break;
        case ActionTriggerType::Held: aj["trigger"] = "Held"; break;
        case ActionTriggerType::DoubleTap: aj["trigger"] = "DoubleTap"; break;
        default: aj["trigger"] = "Pressed"; break;
        }
        j["actions"].push_back(aj);
    }

    j["axes"] = json::array();
    for (auto& ax : axes) {
        json axj;
        axj["axisName"] = ax.axisName;
        axj["positiveKey"] = ax.positiveKey;
        axj["negativeKey"] = ax.negativeKey;
        axj["gamepadAxis"] = ax.gamepadAxis;
        axj["deadzone"] = ax.deadzone;
        axj["sensitivity"] = ax.sensitivity;
        j["axes"].push_back(axj);
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

InputActionMapAsset* InputActionMapAsset::Get(const std::string& path) {
    auto it = s_cache.find(path);
    if (it != s_cache.end()) return &it->second;

    InputActionMapAsset asset;
    if (!asset.LoadFromFile(path)) return nullptr;
    s_cache[path] = std::move(asset);
    return &s_cache[path];
}

void InputActionMapAsset::ClearCache() {
    s_cache.clear();
}
