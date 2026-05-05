#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// Bloom 用のポストエフェクト設定を ECS から RenderContext へ転送するシステム。
// PostEffectComponent に設定されたしきい値や強度を、描画パスが参照する bloomData に反映する。
class BloomExtractSystem {
public:
    // Registry 内の PostEffectComponent を検索し、Bloom 用パラメータを RenderContext に書き込む。
    void Extract(Registry& registry, RenderContext& rc);
};
