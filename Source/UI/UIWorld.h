#pragma once
#include "UIElement.h"
#include <DirectXMath.h>

// Base for HUD elements anchored to a world-space position. Derived
// classes do their own drawing (text via FontManager, sprites via
// SpriteRenderer) after WorldToScreen projects the position. The base
// itself no longer carries a Sprite3D; that legacy DX11 path has been
// removed (HUD_HPBar_Spec_2026-05-05 v3 section 3.7).
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
