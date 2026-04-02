#include "InputBindingProfileAsset.h"
#include "Console/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

bool InputBindingProfileAsset::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json j;
        file >> j;
        baseName = j.value("baseName", "");
        overrides.clear();
        if (j.contains("overrides")) {
            for (auto& o : j["overrides"]) {
                BindingOverride bo;
                bo.actionName = o.value("actionName", "");
                bo.scancode = o.value("scancode", 0u);
                bo.gamepadButton = o.value("gamepadButton", (uint8_t)0xFF);
                overrides.push_back(bo);
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("[InputBindingProfile] Parse error: %s", e.what());
        return false;
    }
    return true;
}

bool InputBindingProfileAsset::SaveToFile(const std::string& path) const {
    json j;
    j["baseName"] = baseName;
    j["overrides"] = json::array();
    for (auto& o : overrides) {
        json oj;
        oj["actionName"] = o.actionName;
        oj["scancode"] = o.scancode;
        oj["gamepadButton"] = o.gamepadButton;
        j["overrides"].push_back(oj);
    }
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}
