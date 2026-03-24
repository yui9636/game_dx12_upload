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

private:
    float progress;
};
