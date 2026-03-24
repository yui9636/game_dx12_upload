#pragma once
#include "UIElement.h"
#include "Sprite/Sprite3D.h"
#include <memory>
#include <DirectXMath.h>

class UIWorld : public UIElement
{
public:
    UIWorld();
    virtual ~UIWorld() = default;

    void Render(const RenderContext& rc) override;

    void SetSprite(std::shared_ptr<Sprite3D> sprite);

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
    std::shared_ptr<Sprite3D> sprite;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 rotation;
    DirectX::XMFLOAT2 size;
    float progress;
    bool isBillboard;
};
