// InputTextSystem の入力処理関連宣言をまとめます。
#pragma once

class Registry;
class InputEventQueue;
class IInputBackend;

class InputTextSystem {
public:
    static void Update(Registry& registry, const InputEventQueue& queue, IInputBackend& backend);
};
