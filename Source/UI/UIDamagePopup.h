#pragma once
#include "UIWorld.h"
#include <DirectXMath.h>

class UIDamagePopup : public UIWorld
{
public:
    UIDamagePopup();
    ~UIDamagePopup() override = default;

    void Update(float dt) override;

    void Render(const RenderContext& rc) override;

    void Setup(const DirectX::XMFLOAT3& pos, int damage);
    bool IsActive() const { return isActive; }

private:
    bool isActive = false;

    DirectX::XMFLOAT3 velocity;

    int damageValue = 0;
    float lifeTime = 0.0f;
    float maxLifeTime = 1.0f;
};
