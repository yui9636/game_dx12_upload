#pragma once
#include "UIWorld.h"

class UIHPNumber : public UIWorld
{
public:
    UIHPNumber();
    ~UIHPNumber() override = default;

    void Render(const RenderContext& rc) override;

    void SetHP(int current, int max);
    void SetScreenOffset(float x, float y);
    void SetScale(float value);

private:
    int currentHP = 0;
    int maxHP = 0;
    DirectX::XMFLOAT2 screenOffset{ 0.0f, -18.0f };
    float scale = 0.2f;
};
