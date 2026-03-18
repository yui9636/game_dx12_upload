#pragma once
#include "UIScreen.h"
#include <memory>

class Sprite; // 前方宣言

class UICombo : public UIScreen
{
public:
    UICombo();
    ~UICombo() override = default;

    void Update(float dt) override;
    void Render(const RenderContext& rc) override;

    // コンボ数をセット（変化があればアニメーション開始）
    void SetCombo(int count);

private:
    int currentCombo = 0;

    // アニメーション用
    float displayTimer = 0.0f;    // 表示残り時間
    float animationTimer = 0.0f;  // 拡大縮小アニメの経過時間
    float shakeTimer = 0.0f;      // 揺れアニメの経過時間

    // 設定値
    const float MAX_DISPLAY_TIME = 3.0f; // コンボ継続時間
    const float ANIMATION_DURATION = 0.3f; // 拡大→戻るまでの時間
    const float SHAKE_DURATION = 0.2f;     // 揺れている時間

    // ゲージ用スプライト
    std::shared_ptr<Sprite> gaugeSprite;
};
