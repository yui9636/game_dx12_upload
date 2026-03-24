#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <d3d11.h>
#include "UIElement.h"

struct RenderContext;

class UIManager
{
private:
    UIManager() = default;
    ~UIManager() = default;

public:
    static UIManager& Instance()
    {
        static UIManager instance;
        return instance;
    }

    template <typename T>
    std::shared_ptr<T> CreateElement()
    {
        auto element = std::make_shared<T>();
        elements.push_back(element);
        return element;
    }

    void RemoveElement(std::shared_ptr<UIElement> element);
    void Clear();

    void Update(float dt);

    void Render(const RenderContext& rc);



private:
    std::vector<std::shared_ptr<UIElement>> elements;
};
