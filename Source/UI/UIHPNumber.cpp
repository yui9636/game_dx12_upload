#include "UIHPNumber.h"
#include "Font/FontManager.h"
#include "RHI/ICommandList.h"

#include <algorithm>

using namespace DirectX;

UIHPNumber::UIHPNumber()
{
}

void UIHPNumber::SetHP(int current, int max)
{
    currentHP = current;
    maxHP = max;
}

void UIHPNumber::SetScreenOffset(float x, float y)
{
    screenOffset = { x, y };
}

void UIHPNumber::SetScale(float value)
{
    scale = (std::max)(0.01f, value);
}

void UIHPNumber::Render(const RenderContext& rc)
{
    if (!IsActive()) return;

    XMFLOAT3 screenPos;
    if (!WorldToScreen(rc, screenPos)) {
        return;
    }

    FontManager::Instance().DrawFormat(
        rc.commandList,
        "ComboFont",
        screenPos.x + screenOffset.x,
        screenPos.y + screenOffset.y,
        color,
        scale,
        FontAlign::Center,
        L"%d / %d",
        currentHP, maxHP
    );
}
