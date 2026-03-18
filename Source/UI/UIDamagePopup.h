#pragma once
#include "UIWorld.h" // 継承元変更
#include <DirectXMath.h>

// UIWorld を継承 (Renderをオーバーライドする)
class UIDamagePopup : public UIWorld
{
public:
    UIDamagePopup();
    ~UIDamagePopup() override = default;

    void Update(float dt) override;

    // ★重要: UIWorldのRender(スプライト描画)を上書きして、文字描画にする
    void Render(const RenderContext& rc) override;

    void Setup(const DirectX::XMFLOAT3& pos, int damage);
    bool IsActive() const { return isActive; }

private:
    bool isActive = false;

    // DirectX::XMFLOAT3 worldPosition; // 削除 (親の position を使う)
    DirectX::XMFLOAT3 velocity;

    int damageValue = 0;
    float lifeTime = 0.0f;
    float maxLifeTime = 1.0f;
};