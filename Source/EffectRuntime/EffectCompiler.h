#pragma once

#include <memory>
#include <string>
#include "EffectGraphAsset.h"
// EffectCompiler はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

class EffectCompiler
{
public:
    static std::shared_ptr<CompiledEffectAsset> Compile(const EffectGraphAsset& asset, const std::string& sourceAssetPath = {});
};
