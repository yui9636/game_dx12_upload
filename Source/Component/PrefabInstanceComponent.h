// PrefabInstanceComponent の ECS コンポーネント定義をまとめます。
#pragma once

#include <string>

struct PrefabInstanceComponent
{
    std::string prefabAssetPath;
    bool hasOverrides = false;
};
