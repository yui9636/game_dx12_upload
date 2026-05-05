#include "UIHPText2D.h"

#include "Graphics.h"
#include "RenderContext/RenderContext.h"

#include <algorithm>

namespace
{
    float ResolveViewportWidth(const RenderContext& rc)
    {
        float width = static_cast<float>(rc.displayWidth);
        if (width <= 0.0f) width = rc.mainViewport.width;
        if (width <= 0.0f) width = Graphics::Instance().GetScreenWidth();
        return (std::max)(1.0f, width);
    }

    float ResolveViewportHeight(const RenderContext& rc)
    {
        float height = static_cast<float>(rc.displayHeight);
        if (height <= 0.0f) height = rc.mainViewport.height;
        if (height <= 0.0f) height = Graphics::Instance().GetScreenHeight();
        return (std::max)(1.0f, height);
    }
}

UIHPText2D::UIHPText2D() = default;

void UIHPText2D::SetHP(int current, int max)
{
    currentHP = current;
    maxHP = max;
}

void UIHPText2D::SetPosition(float x, float y)
{
    useResponsivePosition = false;
    position = { x, y };
}

void UIHPText2D::SetResponsivePosition(float xNorm, float yNorm)
{
    useResponsivePosition = true;
    responsivePosition = { xNorm, yNorm };
}

void UIHPText2D::SetScale(float value)
{
    scale = (std::max)(0.01f, value);
}

void UIHPText2D::SetAlign(FontAlign value)
{
    align = value;
}

void UIHPText2D::Render(const RenderContext& rc)
{
    if (!IsActive()) {
        return;
    }

    DirectX::XMFLOAT2 drawPos = position;
    if (useResponsivePosition) {
        drawPos.x = ResolveViewportWidth(rc) * responsivePosition.x;
        drawPos.y = ResolveViewportHeight(rc) * responsivePosition.y;
    }

    FontManager::Instance().DrawFormat(
        rc.commandList,
        "ComboFont",
        drawPos.x,
        drawPos.y,
        color,
        scale,
        align,
        L"%d / %d",
        currentHP,
        maxHP);
}
