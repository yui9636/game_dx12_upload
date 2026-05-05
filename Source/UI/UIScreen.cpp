#include "UIScreen.h"

#include "RenderContext/RenderContext.h"
#include "Sprite/SpriteRenderer.h"

using namespace DirectX;

UIScreen::UIScreen()
    : position(0, 0), size(100, 100), pivot(0, 0)
{
}

void UIScreen::Render(const RenderContext& /*rc*/)
{
    if (!IsActive() || !sprite) return;

    const XMFLOAT2 globalPos = GetGlobalPosition();
    const float drawX = globalPos.x - (size.x * pivot.x);
    const float drawY = globalPos.y - (size.y * pivot.y);

    SpriteRenderer::Instance().Draw(
        *sprite,
        drawX, drawY,
        size.x, size.y,
        rotation,
        color);
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
