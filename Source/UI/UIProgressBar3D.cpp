#include "UIProgressBar3D.h"
#include "Camera/Camera.h" 
#include <algorithm>
#include "RHI/ICommandList.h"

using namespace DirectX;

UIProgressBar3D::UIProgressBar3D()
    : backgroundColor(0.2f, 0.2f, 0.2f, 0.5f) // デフォルト背景色: 暗いグレー半透明
{
    progress = 1.0f; // 基底クラスのメンバ初期化
}

void UIProgressBar3D::SetProgress(float v)
{
    progress = std::clamp(v, 0.0f, 1.0f);
}

void UIProgressBar3D::SetBackgroundSprite(std::shared_ptr<Sprite3D> sprite)
{
    backgroundSprite = sprite;
}

void UIProgressBar3D::Render(const RenderContext& rc)
{
    if (!visible) return;

    // カメラ情報の取得
    Camera& cam = Camera::Instance();
    XMMATRIX view = XMLoadFloat4x4(&cam.GetView());
    XMMATRIX proj = XMLoadFloat4x4(&cam.GetProjection());

    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

    // 1. 背景の描画 (もしあれば)
    // 背景は常に Progress = 1.0 (フル) で描画します
    if (backgroundSprite)
    {
        // 色を一時的に背景色に上書きして描画することも可能ですが、
        // ここではメンバ変数の backgroundColor を使用します。

        if (isBillboard)
        {
            backgroundSprite->RenderBillboard(
                dc, view, proj, position, size, backgroundColor, 1.0f
            );
        }
        else
        {
            backgroundSprite->Render(
                dc, view, proj, position, rotation, size, backgroundColor, 1.0f
            );
        }
    }

    // 2. 本体の描画
    if (sprite)
    {
        if (isBillboard)
        {
            sprite->RenderBillboard(
                dc, view, proj, position, size, color, progress
            );
        }
        else
        {
            sprite->Render(
                dc, view, proj, position, rotation, size, color, progress
            );
        }
    }
}