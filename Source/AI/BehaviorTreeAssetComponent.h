#pragma once
#include <string>

// Reference to a serialized BehaviorTreeAsset (.bt) file.
// BehaviorTreeSystem loads this lazily and caches it.
struct BehaviorTreeAssetComponent
{
    std::string assetPath;     // e.g. "Data/AI/BehaviorTrees/Knight.bt"
};
