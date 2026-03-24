#include "UIScreen.h"
#include "RenderContext/RenderContext.h"
#include "RHI/ICommandList.h"

using namespace DirectX;

UIScreen::UIScreen()
    : position(0, 0), size(100, 100), pivot(0, 0)
{
}

void UIScreen::Render(const RenderContext& rc)
{
    if (!IsActive() || !sprite) return;

    ID3D11DeviceContext* dc = rc.commandList->GetNativeContext();

    DirectX::XMFLOAT2 globalPos = GetGlobalPosition();
    float drawX = globalPos.x - (size.x * pivot.x);
    float drawY = globalPos.y - (size.y * pivot.y);

    sprite->Render(dc, drawX, drawY, 0.0f, size.x, size.y, rotation, color.x, color.y, color.z, color.w);

}

void UIScreen::SetSprite(std::shared_ptr<Sprite> newSprite)
{
    sprite = newSprite;
    if (sprite)
    {
        size.x = static_cast<float>(sprite->GetTextureWidth());
        size.y = static_cast<float>(sprite->GetTextureHeight());
    }
}

DirectX::XMFLOAT2 UIScreen::GetGlobalPosition() const {
    if (auto p = std::dynamic_pointer_cast<UIScreen>(parent.lock())) {
        DirectX::XMFLOAT2 parentPos = p->GetGlobalPosition();
        return { parentPos.x + position.x, parentPos.y + position.y };
    }
    return position;
}
