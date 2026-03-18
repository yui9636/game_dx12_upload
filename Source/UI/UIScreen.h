#pragma once
#include "UIElement.h"
#include "Sprite/Sprite.h" // 既存の2Dスプライト
#include <memory>

// 画面固定の2D UI (ボタン、メニュー、従来のバーなど)
class UIScreen : public UIElement
{
public:
    UIScreen();
    virtual ~UIScreen() = default;

    void Render(const RenderContext& rc) override;

    // 2Dスプライト設定
    void SetSprite(std::shared_ptr<Sprite> sprite);

    DirectX::XMFLOAT2 GetGlobalPosition() const;

    // --- 2D専用プロパティ ---
    // 座標 (ピクセル単位)
    void SetPosition(float x, float y) { position = { x, y }; }
    const DirectX::XMFLOAT2& GetPosition() const { return position; }

    // サイズ (ピクセル単位)
    void SetSize(float w, float h) { size = { w, h }; }

    const DirectX::XMFLOAT2& GetSize() const { return size; }

    // 基準点 (0.0=左上, 0.5=中心)
    void SetPivot(float x, float y) { pivot = { x, y }; }


    void SetRotation(float angleDeg) { rotation = angleDeg; }
    float GetRotation() const { return rotation; }

    void SetGlow(float r, float g, float b, float intensity)
    {
        if (sprite) sprite->SetGlow({ r, g, b }, intensity);
    }
protected:
    std::shared_ptr<Sprite> sprite;
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 size;
    DirectX::XMFLOAT2 pivot;

    float rotation = 0.0f;
};