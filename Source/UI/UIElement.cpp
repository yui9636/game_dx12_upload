#include "UIElement.h"

using namespace DirectX;

UIElement::UIElement()
    : visible(true)
    , color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

void UIElement::Update(float dt)
{
    // 共通のアニメーション処理があればここに記述
}