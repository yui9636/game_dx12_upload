#pragma once
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"
// ShadowExtractSystem は対象コンポーネントを走査し、対応する実行時更新を担当する。

class ShadowExtractSystem {
public:
    void Extract(Registry& registry, RenderContext& rc);
};
