#pragma once
#include "UIWorld.h"

class UIHPNumber : public UIWorld
{
public:
    UIHPNumber();
    ~UIHPNumber() override = default;

    void Render(const RenderContext& rc) override;

    void SetHP(int current, int max);

private:
    int currentHP = 0;
    int maxHP = 0;

    const float OFFSET_Y = 0.25f;
};
