#include "UIProgressBar2D.h"

#include <algorithm>

#include "Render/Graphics.h"
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

void UIProgressBar2D::SetResponsiveRect(float centerXNorm, float centerYNorm, float widthNorm, float heightPx)
{
    useResponsiveRect = true;
    responsiveRect = { centerXNorm, centerYNorm, widthNorm, heightPx };
}

void UIProgressBar2D::ClearResponsiveRect()
{
    useResponsiveRect = false;
}

void UIProgressBar2D::Render(const RenderContext& rc)
{
    if (!IsActive() || !sprite) return;

    XMFLOAT2 drawPosition = position;
    XMFLOAT2 drawSize = size;
    XMFLOAT2 drawPivot = pivot;
    if (useResponsiveRect) {
        float viewportW = static_cast<float>(rc.displayWidth);
        float viewportH = static_cast<float>(rc.displayHeight);
        if (viewportW <= 0.0f) viewportW = rc.mainViewport.width;
        if (viewportH <= 0.0f) viewportH = rc.mainViewport.height;
        if (viewportW <= 0.0f) viewportW = Graphics::Instance().GetScreenWidth();
        if (viewportH <= 0.0f) viewportH = Graphics::Instance().GetScreenHeight();
        viewportW = (std::max)(1.0f, viewportW);
        viewportH = (std::max)(1.0f, viewportH);

        drawPosition = { viewportW * responsiveRect.x, viewportH * responsiveRect.y };
        drawSize = { viewportW * responsiveRect.z, responsiveRect.w };
        drawPivot = { 0.5f, 0.5f };
    }

    const float originalWidth = drawSize.x;
    const float drawWidth = originalWidth * progress;
    const float drawX = drawPosition.x - (originalWidth * drawPivot.x);
    const float drawY = drawPosition.y - (drawSize.y * drawPivot.y);

    // source rect を進捗率で切り詰めることで、
    // 256x32 のバー画像を進捗幅に比例して矩形へ割り当てる。
    const float srcW = static_cast<float>(sprite->GetTextureWidth()) * progress;
    const float srcH = static_cast<float>(sprite->GetTextureHeight());

    SpriteRenderer::Instance().Draw(
        *sprite,
        drawX, drawY,
        drawWidth, drawSize.y,
        0.0f, 0.0f, srcW, srcH,
        rotation,
        color);
}
