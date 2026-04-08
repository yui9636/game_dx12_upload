#pragma once

#include <memory>
#include <string>
#include "EffectGraphAsset.h"

class EffectCompiler
{
public:
    static std::shared_ptr<CompiledEffectAsset> Compile(const EffectGraphAsset& asset, const std::string& sourceAssetPath = {});
};
