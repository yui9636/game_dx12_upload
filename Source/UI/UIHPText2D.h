#pragma once

#include "UIElement.h"
#include "Font/FontManager.h"

class UIHPText2D : public UIElement
{
public:
    UIHPText2D();
    ~UIHPText2D() override = default;

    void Render(const RenderContext& rc) override;

    void SetHP(int current, int max);
    void SetPosition(float x, float y);
    void SetResponsivePosition(float xNorm, float yNorm);
    void SetScale(float value);
    void SetAlign(FontAlign value);

private:
    bool useResponsivePosition = true;
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 responsivePosition{ 0.5f, 0.5f };
    float scale = 0.3f;
    FontAlign align = FontAlign::Center;
    int currentHP = 0;
    int maxHP = 0;
};
