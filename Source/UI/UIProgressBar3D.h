#pragma once
#include "UIWorld.h"

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

    void SetBackgroundSprite(std::shared_ptr<Sprite3D> sprite);

    void SetBackgroundColor(float r, float g, float b, float a) { backgroundColor = { r, g, b, a }; }

private:
    std::shared_ptr<Sprite3D> backgroundSprite;
    DirectX::XMFLOAT4 backgroundColor;
};
