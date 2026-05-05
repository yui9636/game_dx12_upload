#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// カラーフィルター用のポストエフェクト設定を ECS から RenderContext へ転送するシステム。
// 露光、モノクロ化、色相変化、フラッシュ、ビネットなどの画面補正値を描画側へ渡す。
class ColorFilterExtractSystem {
public:
    // Registry 内の PostEffectComponent を検索し、カラーフィルター用パラメータを RenderContext に書き込む。
    void Extract(Registry& registry, RenderContext& rc);
};
