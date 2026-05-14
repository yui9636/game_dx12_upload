// InputResolveSystem の入力処理関連宣言をまとめます。
#pragma once

class Registry;
class InputEventQueue;

class InputResolveSystem {
public:
    static void Update(Registry& registry, const InputEventQueue& queue, float dt);
};
