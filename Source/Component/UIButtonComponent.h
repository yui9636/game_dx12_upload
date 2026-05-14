#pragma once

#include <string>

// entity を 2D UI button として扱うための印。
// CanvasItemComponent、RectTransformComponent、SpriteComponent と組み合わせる。
// Hierarchy 内で使い、クリック検出は UIButtonClickSystem が行う。
struct UIButtonComponent
{
    // click 完了時に発行する event-style id。
    std::string buttonId;

    // false の間はこの button から click event を発行しない。
    bool enabled = true;
};
