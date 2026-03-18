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
    // 0.0～1.0の範囲に制限
    progress = std::clamp(v, 0.0f, 1.0f);
}

void UIProgressBar2D::Render(const RenderContext& rc)
{
    if (!visible || !sprite) return;

    // 現在のサイズを保存
    float originalWidth = size.x;

    // 進行度に合わせて描画幅を計算
    // ※単純なスケーリング（伸縮）。テクスチャが歪むのが嫌な場合は、
    //   Spriteクラス側でUVカット機能(SourceRect指定)を実装する必要があります。
    float drawWidth = originalWidth * progress;

    // Pivot計算
    float drawX = position.x - (originalWidth * pivot.x); // 位置は元のサイズ基準
    float drawY = position.y - (size.y * pivot.y);

    XMMATRIX view = XMLoadFloat4x4(&rc.viewMatrix);
    XMMATRIX proj = XMLoadFloat4x4(&rc.projectionMatrix);

    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

    sprite->Render(
        dc,
        drawX, drawY,
        0.0f,           // Z
        drawWidth,      // 幅 (progress反映)
        size.y,         // 高さ (そのまま)
        0.0f,           // 回転
        color.x, color.y, color.z, color.w
    );
}