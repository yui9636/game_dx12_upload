#pragma once
#include "UIScreen.h"
#include <RenderContext/RenderContext.h>

// ============================================================================
// UIProgressBar2D
// 
// 概要: 画面固定(Screen Space)のプログレスバー
// 用途: スタミナゲージ、必殺技ゲージ、メニュー画面のパラメータ表示など
// 挙動: Progress値に応じて横幅を伸縮させて描画します
// ============================================================================
class UIProgressBar2D : public UIScreen
{
public:
    UIProgressBar2D();
    virtual ~UIProgressBar2D() = default;

    // 描画オーバーライド
    void Render(const RenderContext& rc) override;

    // 進行度設定 (0.0f ～ 1.0f)
    void SetProgress(float v);
    float GetProgress() const { return progress; }

private:
    float progress;
};