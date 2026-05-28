#include "UIButtonClickSystem.h"

#include "Component/HierarchyComponent.h"
#include "Component/UIButtonComponent.h"
#include "Input/InputEvent.h"
#include "Input/InputEventQueue.h"
#include "Registry/Registry.h"
#include "UI/UIHitTestSystem.h"
#include "UI/UIScreenSpaceOverlay.h"
#include "UIButtonClickEventQueue.h"

namespace
{
    EntityID g_capturedButtonEntity = Entity::NULL_ID;
    std::string g_capturedButtonId;

    struct ButtonTarget
    {
        EntityID entity = Entity::NULL_ID;
        const UIButtonComponent* button = nullptr;
    };

    ButtonTarget ResolveButtonTarget(Registry& registry, EntityID hitEntity)
    {
        ButtonTarget target;
        EntityID cursor = hitEntity;

        while (!Entity::IsNull(cursor) && registry.IsAlive(cursor)) {
            if (const auto* button = registry.GetComponent<UIButtonComponent>(cursor)) {
                if (button->enabled && !button->buttonId.empty()) {
                    target.entity = cursor;
                    target.button = button;
                    return target;
                }
            }

            const auto* hierarchy = registry.GetComponent<HierarchyComponent>(cursor);
            if (!hierarchy) {
                break;
            }
            cursor = hierarchy->parent;
        }

        return target;
    }

    ButtonTarget PickButtonTarget(Registry& registry,
                                  const DirectX::XMFLOAT4& gameViewRect,
                                  const DirectX::XMFLOAT4X4& view,
                                  const DirectX::XMFLOAT4X4& projection,
                                  const DirectX::XMFLOAT2& screenPoint)
    {
        UIHitTestResult overlayHit = UIScreenSpaceOverlay::PickTopmost(
            registry,
            gameViewRect,
            view,
            projection,
            screenPoint);

        if (!Entity::IsNull(overlayHit.entity)) {
            return ResolveButtonTarget(registry, overlayHit.entity);
        }

        UIHitTestResult hit = UIHitTestSystem::PickTopmost(
            registry,
            gameViewRect,
            view,
            projection,
            screenPoint);

        if (Entity::IsNull(hit.entity)) {
            return {};
        }

        return ResolveButtonTarget(registry, hit.entity);
    }
}

void UIButtonClickSystem::ResetCapture()
{
    g_capturedButtonEntity = Entity::NULL_ID;
    g_capturedButtonId.clear();
}

void UIButtonClickSystem::Update(
    Registry&                  gameRegistry,
    UIButtonClickEventQueue&   outQueue,
    const InputEventQueue&     inputQueue,
    const DirectX::XMFLOAT4&   gameViewRect,
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection)
{
    if (gameViewRect.z <= 1.0f || gameViewRect.w <= 1.0f) {
        ResetCapture();
        return;
    }

    for (const auto& ev : inputQueue.GetEvents()) {
        if (ev.type == InputEventType::WindowFocusLost || ev.type == InputEventType::DeviceRemoved) {
            ResetCapture();
            continue;
        }

        if (ev.type != InputEventType::MouseButtonDown &&
            ev.type != InputEventType::MouseButtonUp) {
            continue;
        }
        if (ev.mouseButton.button != 1) {
            continue; // SDL では 1 が left button。
        }

        const DirectX::XMFLOAT2 screenPoint{ ev.mouseButton.x, ev.mouseButton.y };
        ButtonTarget target = PickButtonTarget(
            gameRegistry,
            gameViewRect,
            view,
            projection,
            screenPoint);

        if (ev.type == InputEventType::MouseButtonDown) {
            if (target.button) {
                g_capturedButtonEntity = target.entity;
                g_capturedButtonId = target.button->buttonId;
            } else {
                ResetCapture();
            }
            continue;
        }

        if (!Entity::IsNull(g_capturedButtonEntity) &&
            target.button &&
            target.entity == g_capturedButtonEntity &&
            target.button->buttonId == g_capturedButtonId) {
            outQueue.Push(g_capturedButtonId);
        }
        ResetCapture();
    }
}
