// DodgeSystem のシステム宣言をまとめます。
#pragma once
class Registry;
class DodgeSystem {
public:
    static void Update(Registry& registry, float dt);
};
