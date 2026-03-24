#include "UIWorld.h"
#include "Camera/Camera.h"
#include "RenderContext/RenderContext.h"
#include "Graphics.h"

using namespace DirectX;

UIWorld::UIWorld()
    : position(0, 0, 0)
    , rotation(0, 0, 0)
    , size(1.0f, 1.0f)
    , progress(1.0f)
    , isBillboard(false)
{
}

void UIWorld::Render(const RenderContext& rc)
{
    if (!IsActive() || !sprite) return;

    DirectX::XMFLOAT3 globalPos = position;
    if (auto p = std::dynamic_pointer_cast<UIWorld>(parent.lock())) {
        DirectX::XMFLOAT3 pPos = p->GetPosition();
        globalPos.x += pPos.x;
        globalPos.y += pPos.y;
        globalPos.z += pPos.z;
    }

}

void UIWorld::SetSprite(std::shared_ptr<Sprite3D> newSprite)
{
    sprite = newSprite;
    if (sprite)
    {
        float scale = 0.01f;
        size.x = sprite->GetTextureWidth() * scale;
        size.y = sprite->GetTextureHeight() * scale;
    }
}

bool UIWorld::WorldToScreen(const RenderContext& rc, DirectX::XMFLOAT3& outScreenPos) const
{
   
    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);

    float w = Graphics::Instance().GetScreenWidth();
    float h = Graphics::Instance().GetScreenHeight();

    XMVECTOR targetPos = XMLoadFloat3(&position);

    XMVECTOR screenPosVec = XMVector3Project(
        targetPos,
        0, 0, w, h, 0.0f, 1.0f,
        proj, view, XMMatrixIdentity()
    );

    XMStoreFloat3(&outScreenPos, screenPosVec);

    if (outScreenPos.z < 0.0f || outScreenPos.z > 1.0f) return false;

    return true;
}
