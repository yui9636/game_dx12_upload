#pragma once
#include "UIElement.h"
#include "Sprite/Sprite.h"
#include <memory>

class UIScreen : public UIElement
{
public:
    UIScreen();
    virtual ~UIScreen() = default;

    void Render(const RenderContext& rc) override;

    void SetSprite(std::shared_ptr<Sprite> sprite);

    DirectX::XMFLOAT2 GetGlobalPosition() const;

    void SetPosition(float x, float y) { position = { x, y }; }
    const DirectX::XMFLOAT2& GetPosition() const { return position; }

    void SetSize(float w, float h) { size = { w, h }; }

    const DirectX::XMFLOAT2& GetSize() const { return size; }

    void SetPivot(float x, float y) { pivot = { x, y }; }


    void SetRotation(float angleDeg) { rotation = angleDeg; }
    float GetRotation() const { return rotation; }

    void SetGlow(float r, float g, float b, float intensity)
    {
        if (sprite) sprite->SetGlow({ r, g, b }, intensity);
    }
protected:
    std::shared_ptr<Sprite> sprite;
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 size;
    DirectX::XMFLOAT2 pivot;

    float rotation = 0.0f;
};
