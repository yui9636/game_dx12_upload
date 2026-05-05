#include "UIProgressBar3D.h"

#include "RenderContext/RenderContext.h"
#include "Sprite/SpriteRenderer.h"

#include "RHI/ICommandList.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    bool ProjectWorldPoint(const RenderContext& rc,
                           const DirectX::XMFLOAT3& world,
                           DirectX::XMFLOAT3& outScreen)
    {
        float w = static_cast<float>(rc.displayWidth);
        float h = static_cast<float>(rc.displayHeight);
        if (w <= 0.0f) w = rc.mainViewport.width;
        if (h <= 0.0f) h = rc.mainViewport.height;
        if (w <= 0.0f || h <= 0.0f) return false;

        const XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
        const XMMATRIX proj = XMLoadFloat4x4(&rc.projectionMatrix);
        const XMVECTOR p = XMLoadFloat3(&world);
        const XMVECTOR screen = XMVector3Project(
            p,
            0.0f, 0.0f, w, h, 0.0f, 1.0f,
            proj, view, XMMatrixIdentity());
        XMStoreFloat3(&outScreen, screen);
        return outScreen.z >= 0.0f && outScreen.z <= 1.0f;
    }
}

UIProgressBar3D::UIProgressBar3D()
    : backgroundColor(0.2f, 0.2f, 0.2f, 0.5f)
{
    progress = 1.0f;
}

void UIProgressBar3D::SetProgress(float v)
{
    progress = std::clamp(v, 0.0f, 1.0f);
}

void UIProgressBar3D::SetSprite(std::shared_ptr<Sprite> sprite)
{
    foregroundSprite = std::move(sprite);
}

void UIProgressBar3D::SetBackgroundSprite(std::shared_ptr<Sprite> sprite)
{
    backgroundSprite = std::move(sprite);
}

void UIProgressBar3D::Render(const RenderContext& rc)
{
    if (!visible) return;

    XMFLOAT3 center{};
    if (!WorldToScreen(rc, center)) return;

    float barWidth = size.x;
    float barHeight = size.y;
    if (barWidth <= 10.0f || barHeight <= 10.0f) {
        const XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
        const XMMATRIX invView = XMMatrixInverse(nullptr, view);
        const XMVECTOR right = XMVector3Normalize(invView.r[0]);
        const XMVECTOR up = XMVector3Normalize(invView.r[1]);
        const XMVECTOR origin = XMLoadFloat3(&position);

        XMFLOAT3 leftPos{}, rightPos{}, bottomPos{}, topPos{};
        XMStoreFloat3(&leftPos, origin - right * (size.x * 0.5f));
        XMStoreFloat3(&rightPos, origin + right * (size.x * 0.5f));
        XMStoreFloat3(&bottomPos, origin - up * (size.y * 0.5f));
        XMStoreFloat3(&topPos, origin + up * (size.y * 0.5f));

        XMFLOAT3 leftScreen{}, rightScreen{}, bottomScreen{}, topScreen{};
        if (ProjectWorldPoint(rc, leftPos, leftScreen) && ProjectWorldPoint(rc, rightPos, rightScreen)) {
            barWidth = std::abs(rightScreen.x - leftScreen.x);
        }
        if (ProjectWorldPoint(rc, bottomPos, bottomScreen) && ProjectWorldPoint(rc, topPos, topScreen)) {
            barHeight = std::abs(bottomScreen.y - topScreen.y);
        }
    }

    barWidth = (std::max)(barWidth, 1.0f);
    barHeight = (std::max)(barHeight, 1.0f);

    const float x = center.x - barWidth * 0.5f;
    const float y = center.y - barHeight * 0.5f;

    if (backgroundSprite) {
        SpriteRenderer::Instance().Draw(
            *backgroundSprite,
            x, y,
            barWidth, barHeight,
            0.0f,
            backgroundColor);
    }

    if (foregroundSprite && progress > 0.0f) {
        const float texW = static_cast<float>(foregroundSprite->GetTextureWidth());
        const float texH = static_cast<float>(foregroundSprite->GetTextureHeight());
        SpriteRenderer::Instance().Draw(
            *foregroundSprite,
            x, y,
            barWidth * progress, barHeight,
            0.0f, 0.0f, texW * progress, texH,
            0.0f,
            color);
    }
}
