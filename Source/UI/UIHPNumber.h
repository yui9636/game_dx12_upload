#pragma once
#include "UIWorld.h"

class UIHPNumber : public UIWorld
{
public:
    UIHPNumber();
    ~UIHPNumber() override = default;

    // 描画オーバーライド (3Dフォント描画を使用)
    void Render(const RenderContext& rc) override;

    // 数値セット
    void SetHP(int current, int max);

private:
    int currentHP = 0;
    int maxHP = 0;

    // バーからのオフセット位置 (メートル単位)
    // バーの中心からどれくらい上に表示するか
    const float OFFSET_Y = 0.25f;
};