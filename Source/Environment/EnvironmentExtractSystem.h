#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"

// シーン内の EnvironmentComponent から、描画に必要な環境設定を RenderContext へ反映するシステム。
class EnvironmentExtractSystem {
public:
    // Registry 内の EnvironmentComponent を検索し、IBL や Skybox のパスを RenderContext にコピーする。
    void Extract(Registry& registry, RenderContext& rc);
};
