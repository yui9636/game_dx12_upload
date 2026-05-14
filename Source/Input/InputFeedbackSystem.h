// InputFeedbackSystem の入力処理関連宣言をまとめます。
#pragma once

class Registry;
class IInputBackend;

class InputFeedbackSystem {
public:
    static void Update(Registry& registry, IInputBackend& backend, float dt);
};
