#include "UIButtonClickSystem.h"

#include "Component/UIButtonComponent.h"
#include "Input/InputEvent.h"
#include "Input/InputEventQueue.h"
#include "Registry/Registry.h"
#include "UI/UIHitTestSystem.h"
#include "UIButtonClickEventQueue.h"

void UIButtonClickSystem::Update(
    Registry&                  gameRegistry,
    UIButtonClickEventQueue&   outQueue,
    const InputEventQueue&     inputQueue,
    const DirectX::XMFLOAT4&   gameViewRect,
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection)
{
    if (gameViewRect.z <= 1.0f || gameViewRect.w <= 1.0f) {
        return;
    }

    for (const auto& ev : inputQueue.GetEvents()) {
        if (ev.type != InputEventType::MouseButtonDown) continue;
        if (ev.mouseButton.button != 1)                continue; // SDL: 1 = left

        const DirectX::XMFLOAT2 screenPoint{ ev.mouseButton.x, ev.mouseButton.y };

        UIHitTestResult hit = UIHitTestSystem::PickTopmost(
            gameRegistry,
            gameViewRect,
            view,
            projection,
            screenPoint);

        if (Entity::IsNull(hit.entity)) continue;

        auto* button = gameRegistry.GetComponent<UIButtonComponent>(hit.entity);
        if (!button)         continue;
        if (!button->enabled) continue;
        if (button->buttonId.empty()) continue;

        outQueue.Push(button->buttonId);
    }
}
