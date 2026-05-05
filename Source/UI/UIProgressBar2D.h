#pragma once
#include "UIScreen.h"
#include <RenderContext/RenderContext.h>

// ============================================================================
// UIProgressBar2D
// 
// ============================================================================
class UIProgressBar2D : public UIScreen
{
public:
    UIProgressBar2D();
    virtual ~UIProgressBar2D() = default;

    void Render(const RenderContext& rc) override;

    void SetProgress(float v);
    float GetProgress() const { return progress; }

    void SetResponsiveRect(float centerXNorm, float centerYNorm, float widthNorm, float heightPx);
    void ClearResponsiveRect();

private:
    float progress;
    bool useResponsiveRect = false;
    DirectX::XMFLOAT4 responsiveRect{ 0.5f, 0.5f, 0.4f, 16.0f };
};
