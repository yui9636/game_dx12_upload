#pragma once

#include <string>
#include "EffectGraphAsset.h"

class EffectGraphSerializer
{
public:
    static bool Save(const std::string& path, const EffectGraphAsset& asset);
    static bool Load(const std::string& path, EffectGraphAsset& outAsset);
};
