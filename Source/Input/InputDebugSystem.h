// InputDebugSystem の入力処理関連宣言をまとめます。
#pragma once
#include "InputEventQueue.h"

class Registry;
class IInputBackend;

class InputDebugSystem {
public:
    static void Update(Registry& registry, const InputEventQueue& queue);
    static void DrawDebugWindow(Registry& registry, IInputBackend& backend, const InputEventQueue& queue);
};
