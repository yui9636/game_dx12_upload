// UIWorld の UI 関連実装をまとめます。
#include "UIWorld.h"
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
    (void)rc;
}

bool UIWorld::WorldToScreen(const RenderContext& rc, DirectX::XMFLOAT3& outScreenPos) const
{
   
    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);

    float w = static_cast<float>(rc.displayWidth);
    float h = static_cast<float>(rc.displayHeight);
    if (w <= 0.0f) w = rc.mainViewport.width;
    if (h <= 0.0f) h = rc.mainViewport.height;
    if (w <= 0.0f) w = Graphics::Instance().GetScreenWidth();
    if (h <= 0.0f) h = Graphics::Instance().GetScreenHeight();

    DirectX::XMFLOAT3 globalPos = position;
    if (auto p = std::dynamic_pointer_cast<UIWorld>(parent.lock())) {
        const DirectX::XMFLOAT3 pPos = p->GetPosition();
        globalPos.x += pPos.x;
        globalPos.y += pPos.y;
        globalPos.z += pPos.z;
    }

    XMVECTOR targetPos = XMLoadFloat3(&globalPos);

    XMVECTOR screenPosVec = XMVector3Project(
        targetPos,
        0, 0, w, h, 0.0f, 1.0f,
        proj, view, XMMatrixIdentity()
    );

    XMStoreFloat3(&outScreenPos, screenPosVec);

    if (outScreenPos.z < 0.0f || outScreenPos.z > 1.0f) return false;

    return true;
}
