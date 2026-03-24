#pragma once
#include "Component/Component.h"
#include "UI/UICombo.h"
#include <memory>
#include <UI\UIHPNumber.h>

class UIProgressBar3D;
class Sprite3D;
class UIScreen; 
class Sprite;

class C_PlayerHUD : public Component
{
public:
    const char* GetName() const override { return "PlayerHUD"; }

    void Start() override;
    void Update(float dt) override;

private:
    std::shared_ptr<UIProgressBar3D> hpBar;

    std::shared_ptr<Sprite3D> barSprite;

    std::shared_ptr<UIHPNumber> hpNumber;

    std::shared_ptr<UICombo> comboUI;

    std::shared_ptr<UIScreen> attackIcon;
    std::shared_ptr<Sprite> attackSprite;
};
