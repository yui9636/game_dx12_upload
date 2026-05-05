#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// モーションブラー用のポストエフェクト設定を ECS から RenderContext へ転送するシステム。
// ブラー強度とサンプル数を描画パスが使える形に変換する。
class MotionBlurExtractSystem {
public:
    // Registry 内の PostEffectComponent を検索し、MotionBlur 用パラメータを RenderContext に書き込む。
    void Extract(Registry& registry, RenderContext& rc);
};
