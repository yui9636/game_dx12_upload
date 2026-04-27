#pragma once

#include <string>

// Marks an entity as a 2D UI button.
// Combine with CanvasItemComponent + RectTransformComponent + SpriteComponent
// in a Hierarchy. Click detection is done by UIButtonClickSystem.
struct UIButtonComponent
{
    // Unique id. Must match GameLoopCondition::UIButtonClicked.targetName.
    std::string buttonId;

    // While false, no click event is emitted for this button.
    bool enabled = true;
};
