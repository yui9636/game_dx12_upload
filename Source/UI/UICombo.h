#pragma once
#include "UIScreen.h"
#include <memory>

class Sprite;

class UICombo : public UIScreen
{
public:
    UICombo();
    ~UICombo() override = default;

    void Update(float dt) override;
    void Render(const RenderContext& rc) override;

    void SetCombo(int count);

private:
    int currentCombo = 0;

    float displayTimer = 0.0f;
    float animationTimer = 0.0f;
    float shakeTimer = 0.0f;

    const float MAX_DISPLAY_TIME = 3.0f;
    const float ANIMATION_DURATION = 0.3f;
    const float SHAKE_DURATION = 0.2f;

    std::shared_ptr<Sprite> gaugeSprite;
};
