#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// ECSから影の設定を抽出し、RenderContext(共通環境)に書き込む専用システム
class ShadowExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};