#pragma once
#include "Registry/Registry.h"
// DebugRenderSystem は対象コンポーネントを走査し、対応する実行時更新を担当する。


class DebugRenderSystem {
public:
    void Render(Registry& registry);
};