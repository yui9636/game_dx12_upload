#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// 被写界深度(DoF) 用のポストエフェクト設定を ECS から RenderContext へ転送するシステム。
// 焦点距離やぼけ半径などを描画パスが参照できる形にまとめる。
class DoFExtractSystem {
public:
    // Registry 内の PostEffectComponent を検索し、DoF 用パラメータを RenderContext に書き込む。
    void Extract(Registry& registry, RenderContext& rc);
};
