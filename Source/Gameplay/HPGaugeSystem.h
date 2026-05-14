// HPGaugeSystem のシステム宣言をまとめます。
#pragma once

class Registry;

class HPGaugeSystem
{
public:
    static void Update(Registry& registry, float deltaTime);
};
