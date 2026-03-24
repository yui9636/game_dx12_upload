#include "UIProgressBar2D.h"
#include <algorithm> // for std::clamp
#include "RHI/ICommandList.h"

using namespace DirectX;


UIProgressBar2D::UIProgressBar2D()
    : progress(1.0f)
{
}

void UIProgressBar2D::SetProgress(float v)
{
    progress = std::clamp(v, 0.0f, 1.0f);
}

void UIProgressBar2D::Render(const RenderContext& rc)
{
    if (!visible || !sprite) return;

    float originalWidth = size.x;

    float drawWidth = originalWidth * progress;

    float drawX = position.x - (originalWidth * pivot.x);
    float drawY = position.y - (size.y * pivot.y);

    XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
    XMMATRIX proj = XMLoadFloat4x4(&rc.projectionMatrix);

    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

    sprite->Render(
        dc,
        drawX, drawY,
        0.0f,           // Z
        drawWidth,
        size.y,
        0.0f,
        color.x, color.y, color.z, color.w
    );
}
