// HealthSystem のシステム宣言をまとめます。
#pragma once
class Registry;
class HealthSystem {
public:
    static void Update(Registry& registry, float dt);
};
