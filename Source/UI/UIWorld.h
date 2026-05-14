#pragma once
#include "UIElement.h"
#include <DirectXMath.h>

// ワールド座標へ固定される HUD 要素の基底クラス。派生クラスは
// WorldToScreen で投影した後、テキストは FontManager、スプライトは
// SpriteRenderer を使って各自で描画する。基底クラス自体は
// Sprite3D を持たず、古い DX11 経路は
// HUD_HPBar_Spec_2026-05-05 v3 の 3.7 節に合わせて削除済み。
class UIWorld : public UIElement
{
public:
    UIWorld();
    virtual ~UIWorld() = default;

    void Render(const RenderContext& rc) override;

    void SetPosition(const DirectX::XMFLOAT3& pos) { position = pos; }
    void SetPosition(float x, float y, float z) { position = { x, y, z }; }
    const DirectX::XMFLOAT3& GetPosition() const { return position; }

    void SetRotation(const DirectX::XMFLOAT3& rot) { rotation = rot; }
    void SetRotation(float x, float y, float z) { rotation = { x, y, z }; }

    void SetSize(float w, float h) { size = { w, h }; }
    void SetSize(const DirectX::XMFLOAT2& s) { size = s; }

    void SetBillboard(bool enable) { isBillboard = enable; }
    void SetProgress(float v) { progress = v; }

protected:
    bool WorldToScreen(const RenderContext& rc, DirectX::XMFLOAT3& outScreenPos) const;

protected:
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation;
    DirectX::XMFLOAT2 size;
    float progress;
    bool isBillboard;
};
