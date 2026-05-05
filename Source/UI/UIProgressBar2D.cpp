#include "UIProgressBar2D.h"

#include <algorithm>

#include "RenderContext/RenderContext.h"
#include "Sprite/Sprite.h"
#include "Sprite/SpriteRenderer.h"

using namespace DirectX;

UIProgressBar2D::UIProgressBar2D()
    : progress(1.0f)
{
}

void UIProgressBar2D::SetProgress(float v)
{
    progress = std::clamp(v, 0.0f, 1.0f);
}

void UIProgressBar2D::Render(const RenderContext& /*rc*/)
{
    if (!visible || !sprite) return;

    const float originalWidth = size.x;
    const float drawWidth = originalWidth * progress;
    const float drawX = position.x - (originalWidth * pivot.x);
    const float drawY = position.y - (size.y * pivot.y);

    // Source rect crops the texture to the progress fraction so a full
    // 256x32 bar texture maps proportionally onto a half-width quad.
    const float srcW = static_cast<float>(sprite->GetTextureWidth()) * progress;
    const float srcH = static_cast<float>(sprite->GetTextureHeight());

    SpriteRenderer::Instance().Draw(
        *sprite,
        drawX, drawY,
        drawWidth, size.y,
        0.0f, 0.0f, srcW, srcH,
        rotation,
        color);
}
