#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// モーションブラーのパラメータをECSから抽出する専門業者
class MotionBlurExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};