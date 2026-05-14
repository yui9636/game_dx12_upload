// CharacterPhysicsSystem のシステム宣言をまとめます。
#pragma once
class Registry;
class CharacterPhysicsSystem {
public:
    static void Update(Registry& registry, float dt);
};
