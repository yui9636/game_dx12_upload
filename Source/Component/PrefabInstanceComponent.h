#pragma once

#include <string>

struct PrefabInstanceComponent
{
    std::string prefabAssetPath;
    bool hasOverrides = false;
};
