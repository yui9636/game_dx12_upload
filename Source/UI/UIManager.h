#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <d3d11.h>
#include "UIElement.h"

// 前方宣言
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

    // UI要素の追加
    template <typename T>
    std::shared_ptr<T> CreateElement()
    {
        auto element = std::make_shared<T>();
        elements.push_back(element);
        return element;
    }

    void RemoveElement(std::shared_ptr<UIElement> element);
    void Clear();

    // 更新
    void Update(float dt);

    // 描画
    // ★変更: RenderContextを受け取る
    void Render(const RenderContext& rc);



private:
    std::vector<std::shared_ptr<UIElement>> elements;
};