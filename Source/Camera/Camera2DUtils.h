#pragma once

#include <algorithm>

#include <DirectXMath.h>

#include "Component/Camera2DComponent.h"
#include "Component/Camera2DMainTagComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "Entity/Entity.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

namespace Camera2DUtils
{
// ActiveCamera2D は entity を中心に、実行時やエディターで共有する状態を保持する。
    struct ActiveCamera2D
    {
        EntityID entity = Entity::NULL_ID;
        Camera2DComponent* camera = nullptr;
        TransformComponent* transform = nullptr;
    };

    inline ActiveCamera2D FindActiveCamera2D(Registry& registry)
    {
        ActiveCamera2D best{};
        ActiveCamera2D bestMain{};
        int bestPriority = 0;
        bool foundAny = false;
        int activeMainCount = 0;

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<Camera2DComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
                continue;
            }

            auto* cameraColumn    = archetype->GetColumn(TypeManager::GetComponentTypeID<Camera2DComponent>());
            auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
            auto* hierarchyColumn = signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<HierarchyComponent>())
                : nullptr;
            const bool hasMainTag = signature.test(TypeManager::GetComponentTypeID<Camera2DMainTagComponent>());
            const auto& entities = archetype->GetEntities();

            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                auto* cam   = static_cast<Camera2DComponent*>(cameraColumn->Get(i));
                auto* xform = static_cast<TransformComponent*>(transformColumn->Get(i));
                auto* hier  = hierarchyColumn ? static_cast<HierarchyComponent*>(hierarchyColumn->Get(i)) : nullptr;
                if (!cam || !xform) continue;
                if (hier && !hier->isActive) continue;

                const EntityID e = entities[i];

                if (hasMainTag) {
                    ++activeMainCount;
                    if (Entity::IsNull(bestMain.entity) ||
                        Entity::GetIndex(e) < Entity::GetIndex(bestMain.entity)) {
                        bestMain.entity    = e;
                        bestMain.camera    = cam;
                        bestMain.transform = xform;
                    }
                    continue;
                }

                if (!foundAny ||
                    cam->priority > bestPriority ||
                    (cam->priority == bestPriority &&
                     Entity::GetIndex(e) < Entity::GetIndex(best.entity))) {
                    best.entity    = e;
                    best.camera    = cam;
                    best.transform = xform;
                    bestPriority   = cam->priority;
                    foundAny       = true;
                }
            }
        }

        if (activeMainCount > 1) {
            static bool s_loggedMultipleMainCamera2D = false;
            if (!s_loggedMultipleMainCamera2D) {
                LOG_WARN("[Camera2D] Multiple active Camera2DMainTagComponent instances found; using the first entity.");
                s_loggedMultipleMainCamera2D = true;
            }
        }

        return !Entity::IsNull(bestMain.entity) ? bestMain : best;
    }

    inline void BuildViewProjection(const Camera2DComponent& camera,
                                    const TransformComponent& transform,
                                    const DirectX::XMFLOAT4& viewRect,
                                    DirectX::XMFLOAT4X4& outView,
                                    DirectX::XMFLOAT4X4& outProjection)
    {
        using namespace DirectX;

        const XMVECTOR eye = XMVectorSet(transform.worldPosition.x,
                                         transform.worldPosition.y,
                                         transform.worldPosition.z,
                                         1.0f);
        const XMMATRIX view = XMMatrixLookToLH(eye, XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));

        const float viewportAspect = (viewRect.w > 0.0f) ? (viewRect.z / viewRect.w) : (16.0f / 9.0f);
        const float refW = static_cast<float>((std::max)(camera.referenceResolution.x, 1u));
        const float refH = static_cast<float>((std::max)(camera.referenceResolution.y, 1u));
        const float refAspect = refW / refH;
        const float zoom = (std::max)(camera.zoom, 0.01f);
        const float orthoSize = (std::max)(camera.orthographicSize / zoom, 0.01f);

        float halfWidth  = 0.0f;
        float halfHeight = 0.0f;

        switch (camera.aspectPolicy) {
        case Camera2DComponent::AspectPolicy::Fit:
            if (viewportAspect >= refAspect) {
                halfHeight = orthoSize;
                halfWidth  = halfHeight * viewportAspect;
            } else {
                halfWidth  = orthoSize * refAspect;
                halfHeight = halfWidth / viewportAspect;
            }
            break;
        case Camera2DComponent::AspectPolicy::Fill:
            if (viewportAspect >= refAspect) {
                halfWidth  = orthoSize * refAspect;
                halfHeight = halfWidth / viewportAspect;
            } else {
                halfHeight = orthoSize;
                halfWidth  = halfHeight * viewportAspect;
            }
            break;
        case Camera2DComponent::AspectPolicy::Disabled:
        default:
            halfHeight = orthoSize;
            halfWidth  = halfHeight * viewportAspect;
            break;
        }

        const XMMATRIX projection = XMMatrixOrthographicLH(halfWidth * 2.0f,
                                                           halfHeight * 2.0f,
                                                           camera.nearZ,
                                                           camera.farZ);
        XMStoreFloat4x4(&outView, view);
        XMStoreFloat4x4(&outProjection, projection);
    }

    inline bool TryBuildActiveViewProjection(Registry& registry,
                                             const DirectX::XMFLOAT4& viewRect,
                                             DirectX::XMFLOAT4X4& outView,
                                             DirectX::XMFLOAT4X4& outProjection)
    {
        if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
            return false;
        }
        const ActiveCamera2D active = FindActiveCamera2D(registry);
        if (!active.camera || !active.transform) {
            return false;
        }
        BuildViewProjection(*active.camera, *active.transform, viewRect, outView, outProjection);
        return true;
    }
}
