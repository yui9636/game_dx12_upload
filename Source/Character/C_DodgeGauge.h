#pragma once
#include "Component/Component.h"
#include <memory>
#include <DirectXMath.h>

class UIScreen;
class Sprite;

class C_DodgeGauge : public Component
{
public:
    C_DodgeGauge();
    ~C_DodgeGauge() override;

    const char* GetName() const override { return "DodgeGauge"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    bool TryConsumeStamina();

private:
    void UpdateLayout();
    void UpdateState();

private:
    float currentStamina = 1000.0f;
    const float MAX_STAMINA = 1000.0f;
    const float COST_PER_DODGE = 250.0f;

    const float RECOVERY_RATE = 200.0f;
    const float RECOVERY_DELAY = 0.8f;
    float recoveryTimer = 0.0f;

    std::shared_ptr<UIScreen> centerIcon;
    std::shared_ptr<UIScreen> segments[4];

    std::shared_ptr<Sprite> iconSprite;
    std::shared_ptr<Sprite> gaugeSprite;

    DirectX::XMFLOAT2 uiPosition;

    DirectX::XMFLOAT2 iconSize;

    DirectX::XMFLOAT2 gaugeSize;
    DirectX::XMFLOAT2 gaugePivot;

    DirectX::XMFLOAT2 gaugeOffset;

    float gap = 0.0f;
};
