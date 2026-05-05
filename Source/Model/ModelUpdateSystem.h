#pragma once
// ModelUpdateSystem の公開インターフェースを定義するヘッダです。
#include "Registry/Registry.h"

// ECS 上の Model を毎フレーム更新するシステムです。
class ModelUpdateSystem {
public:
    // Registry 内の Model 更新を実行します。
    static void Update(Registry& registry);
};
