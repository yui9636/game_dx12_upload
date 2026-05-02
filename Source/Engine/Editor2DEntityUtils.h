#pragma once

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/TransformComponent.h"
#include "Entity/Entity.h"
#include "Hierarchy/HierarchySystem.h"
#include "Registry/Registry.h"

namespace Editor2D
{
    inline void SyncRectTransformToTransform(const RectTransformComponent& rect, TransformComponent& transform)
    {
        using namespace DirectX;
        transform.localPosition = { rect.anchoredPosition.x, rect.anchoredPosition.y, 0.0f };
        const XMVECTOR q = XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, XMConvertToRadians(rect.rotationZ));
        XMStoreFloat4(&transform.localRotation, q);
        transform.localScale = {
            (std::max)(std::fabs(rect.scale2D.x), 0.001f),
            (std::max)(std::fabs(rect.scale2D.y), 0.001f),
            1.0f
        };
        transform.isDirty = true;
    }

    inline bool FinalizeCreatedEntity(Registry& registry, EntityID entity)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return false;
        }

        if (!registry.GetComponent<TransformComponent>(entity)) {
            TransformComponent transform{};
            transform.localScale = { 1.0f, 1.0f, 1.0f };
            transform.isDirty = true;
            registry.AddComponent(entity, transform);
        }
        if (!registry.GetComponent<HierarchyComponent>(entity)) {
            registry.AddComponent(entity, HierarchyComponent{});
        }
        if (!registry.GetComponent<RectTransformComponent>(entity)) {
            RectTransformComponent rect{};
            rect.sizeDelta = { 128.0f, 128.0f };
            registry.AddComponent(entity, rect);
        }
        if (!registry.GetComponent<CanvasItemComponent>(entity)) {
            registry.AddComponent(entity, CanvasItemComponent{});
        }

        auto* rect = registry.GetComponent<RectTransformComponent>(entity);
        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!rect || !transform) {
            return false;
        }

        SyncRectTransformToTransform(*rect, *transform);
        HierarchySystem::MarkDirtyRecursive(entity, registry);
        HierarchySystem hierarchySystem;
        hierarchySystem.Update(registry);
        return true;
    }
}
