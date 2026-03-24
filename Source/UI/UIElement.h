#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <algorithm>
#include "RenderContext\RenderContext.h"

class UIElement : public std::enable_shared_from_this<UIElement>
{
public:
    UIElement();
    virtual ~UIElement() = default;

    virtual void Update(float dt);
    virtual void Render(const RenderContext& rc) = 0;

    void AddChild(std::shared_ptr<UIElement> child) {
        if (child) {
            child->parent = shared_from_this();
            children.push_back(child);
        }
    }

    void RemoveChild(std::shared_ptr<UIElement> child) {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }

    void SetVisible(bool v) { visible = v; }

    bool IsActive() const {
        if (!visible) return false;
        if (auto p = parent.lock()) {
            return p->IsActive();
        }
        return true;
    }

    void SetColor(float r, float g, float b, float a) { color = { r, g, b, a }; }
    const DirectX::XMFLOAT4& GetColor() const { return color; }

protected:
    bool visible;
    DirectX::XMFLOAT4 color;

    std::weak_ptr<UIElement> parent;
    std::vector<std::shared_ptr<UIElement>> children;
};
