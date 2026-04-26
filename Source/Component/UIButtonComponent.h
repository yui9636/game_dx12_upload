#pragma once

#include <string>

// 2D UI Button を識別する component。
// CanvasItemComponent + RectTransformComponent + SpriteComponent と
// 組み合わせて Hierarchy 上で button entity を構成する。
// click 検出は UIButtonClickSystem が行う。
struct UIButtonComponent
{
    // 一意な ID。GameLoopCondition::UIButtonClicked.targetName と一致させる。
    std::string buttonId;

    // false の間は click event を発行しない。
    bool enabled = true;
};
