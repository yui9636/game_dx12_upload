#include "UIHPNumber.h"
#include "Font/FontManager.h"
#include "Camera/Camera.h"
#include "RHI/ICommandList.h"

using namespace DirectX;

UIHPNumber::UIHPNumber()
{
}

void UIHPNumber::SetHP(int current, int max)
{
    currentHP = current;
    maxHP = max;
}

void UIHPNumber::Render(const RenderContext& rc)
{
    if (!visible) return;

    Camera& cam = Camera::Instance();
    XMMATRIX view = XMLoadFloat4x4(&cam.GetView());
    XMMATRIX projection = XMLoadFloat4x4(&cam.GetProjection());

    XMFLOAT3 drawPos = position;


    XMFLOAT4 textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float scale = 0.055f;

    FontManager::Instance().DrawFormat3D(
        rc.commandList,
        view,
        projection,
        "ComboFont",
        drawPos,
        rotation,
        scale,
        textColor,
        FontAlign::Center,
        L"%d / %d",
        currentHP, maxHP
    );

}
