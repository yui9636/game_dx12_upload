#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// ECSから環境(空、IBL)の設定を抽出し、RenderContext(共通環境)に書き込む専用システム
class EnvironmentExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};