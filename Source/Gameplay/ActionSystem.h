// ActionSystem のシステム宣言をまとめます。
#pragma once
class Registry;
class ActionSystem {
public:
    static void Update(Registry& registry, float dt);
};
