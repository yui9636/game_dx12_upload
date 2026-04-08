#pragma once

#include <string>

struct EffectAssetComponent
{
    std::string assetPath;
    bool autoPlay = true;
    bool loop = true;
    bool useSelectedMeshFallback = true;
};
