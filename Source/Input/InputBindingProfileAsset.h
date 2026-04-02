#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct BindingOverride {
    std::string actionName;
    uint32_t scancode = 0;
    uint8_t gamepadButton = 0xFF;
};

struct InputBindingProfileAsset {
    std::string baseName;
    std::vector<BindingOverride> overrides;

    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;
};
