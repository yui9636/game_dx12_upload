#pragma once
#include "UIWorld.h"
#include "Sprite/Sprite.h"
#include <memory>

// ============================================================================
// UIProgressBar3D
// 
// ============================================================================
class UIProgressBar3D : public UIWorld
{
public:
    UIProgressBar3D();
    virtual ~UIProgressBar3D() = default;

    void Render(const RenderContext& rc) override;

    void SetProgress(float v);
    float GetProgress() const { return progress; }

    void SetSprite(std::shared_ptr<Sprite> sprite);
    void SetBackgroundSprite(std::shared_ptr<Sprite> sprite);

    void SetBackgroundColor(float r, float g, float b, float a) { backgroundColor = { r, g, b, a }; }

private:
    std::shared_ptr<Sprite> foregroundSprite;
    std::shared_ptr<Sprite> backgroundSprite;
    DirectX::XMFLOAT4 backgroundColor;
};
