#include "UIEditor/UIRectEvaluator.h"

#include <algorithm>
#include <cmath>

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "Undo/EntitySnapshot.h"

namespace
{
    bool AncestorsVisible(Registry& registry, EntityID entity)
    {
        auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        EntityID parent = hierarchy ? hierarchy->parent : Entity::NULL_ID;
        for (int guard = 0; guard < 64 && !Entity::IsNull(parent) && registry.IsAlive(parent); ++guard) {
            if (auto* parentHierarchy = registry.GetComponent<HierarchyComponent>(parent)) {
                if (!parentHierarchy->isActive) {
                    return false;
                }
                if (auto* parentCanvas = registry.GetComponent<CanvasItemComponent>(parent)) {
                    if (!parentCanvas->visible) {
                        return false;
                    }
                }
                parent = parentHierarchy->parent;
            } else {
                break;
            }
        }
        return true;
    }
}

namespace UIEditor
{
    bool UIRectEvaluator::Evaluate(Registry& registry, EntityID entity, UIEditorRect& outRect)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return false;
        }

        auto* rect = registry.GetComponent<RectTransformComponent>(entity);
        auto* canvas = registry.GetComponent<CanvasItemComponent>(entity);
        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!rect || !canvas || !transform || !canvas->visible || !AncestorsVisible(registry, entity)) {
            return false;
        }

        outRect = UIEditorRect{};
        outRect.entity = entity;
        outRect.name = registry.GetComponent<NameComponent>(entity);
        outRect.hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        outRect.transform = transform;
        outRect.rect = rect;
        outRect.canvas = canvas;
        outRect.sprite = registry.GetComponent<SpriteComponent>(entity);
        outRect.text = registry.GetComponent<TextComponent>(entity);
        outRect.center = { transform->worldPosition.x, transform->worldPosition.y };
        outRect.size = {
            rect->sizeDelta.x * (std::max)(0.001f, std::fabs(transform->worldScale.x)),
            rect->sizeDelta.y * (std::max)(0.001f, std::fabs(transform->worldScale.y))
        };
        outRect.pivot = rect->pivot;
        outRect.sortingLayer = canvas->sortingLayer;
        outRect.orderInLayer = canvas->orderInLayer;
        outRect.z = transform->worldPosition.z;
        return true;
    }

    std::vector<UIEditorRect> UIRectEvaluator::CollectCanvasWidgets(Registry& registry, EntityID canvas)
    {
        std::vector<UIEditorRect> result;
        if (Entity::IsNull(canvas) || !registry.IsAlive(canvas)) {
            return result;
        }

        std::vector<EntityID> entities;
        EntitySnapshot::CollectHierarchy(canvas, registry, entities);
        for (EntityID entity : entities) {
            if (entity == canvas) {
                continue;
            }
            UIEditorRect rect;
            if (Evaluate(registry, entity, rect)) {
                result.push_back(rect);
            }
        }

        std::sort(result.begin(), result.end(), [](const UIEditorRect& a, const UIEditorRect& b) {
            if (a.sortingLayer != b.sortingLayer) return a.sortingLayer < b.sortingLayer;
            if (a.orderInLayer != b.orderInLayer) return a.orderInLayer < b.orderInLayer;
            if (std::fabs(a.z - b.z) > 0.0001f) return a.z < b.z;
            return a.entity < b.entity;
        });
        return result;
    }
}
