#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

#include <DirectXMath.h>

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/TransformComponent.h"
#include "Entity/Entity.h"
#include "Registry/Registry.h"

struct UIHitTestResult
{
    EntityID entity = Entity::NULL_ID;
    DirectX::XMFLOAT3 worldPoint = { 0.0f, 0.0f, 0.0f };
};

class UIHitTestSystem
{
public:
    static bool ScreenToCanvasPoint(const DirectX::XMFLOAT4& viewRect,
                                    const DirectX::XMFLOAT4X4& view,
                                    const DirectX::XMFLOAT4X4& projection,
                                    const DirectX::XMFLOAT2& screenPoint,
                                    DirectX::XMFLOAT3& outWorldPoint)
    {
        using namespace DirectX;

        if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
            return false;
        }

        const float localX = (screenPoint.x - viewRect.x) / viewRect.z;
        const float localY = (screenPoint.y - viewRect.y) / viewRect.w;
        if (localX < 0.0f || localX > 1.0f || localY < 0.0f || localY > 1.0f) {
            return false;
        }

        const float ndcX = localX * 2.0f - 1.0f;
        const float ndcY = 1.0f - localY * 2.0f;

        const XMMATRIX inverseViewProj = XMMatrixInverse(nullptr,
            XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));
        const XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverseViewProj);
        const XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverseViewProj);

        const float nearZ = XMVectorGetZ(nearPoint);
        const float farZ = XMVectorGetZ(farPoint);
        const float deltaZ = farZ - nearZ;
        if (std::fabs(deltaZ) < 0.000001f) {
            return false;
        }

        const float t = -nearZ / deltaZ;
        if (t < 0.0f) {
            return false;
        }

        const XMVECTOR point = XMVectorLerp(nearPoint, farPoint, t);
        XMStoreFloat3(&outWorldPoint, point);
        return true;
    }

    static bool ComputeScreenCorners(const TransformComponent& transform,
                                     const RectTransformComponent& rect,
                                     const DirectX::XMFLOAT4& viewRect,
                                     const DirectX::XMFLOAT4X4& view,
                                     const DirectX::XMFLOAT4X4& projection,
                                     std::array<DirectX::XMFLOAT2, 4>& outCorners)
    {
        using namespace DirectX;

        const float minX = -rect.pivot.x * rect.sizeDelta.x;
        const float maxX = (1.0f - rect.pivot.x) * rect.sizeDelta.x;
        const float minY = -rect.pivot.y * rect.sizeDelta.y;
        const float maxY = (1.0f - rect.pivot.y) * rect.sizeDelta.y;

        const XMVECTOR center = XMLoadFloat3(&transform.worldPosition);
        const XMVECTOR rotation = XMLoadFloat4(&transform.worldRotation);
        const XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(rotation);
        const XMVECTOR scale = XMLoadFloat3(&transform.worldScale);

        const std::array<XMFLOAT3, 4> localCorners = {
            XMFLOAT3{ minX, minY, 0.0f },
            XMFLOAT3{ maxX, minY, 0.0f },
            XMFLOAT3{ maxX, maxY, 0.0f },
            XMFLOAT3{ minX, maxY, 0.0f }
        };

        const XMMATRIX viewProj = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection);
        for (size_t i = 0; i < localCorners.size(); ++i) {
            XMVECTOR p = XMLoadFloat3(&localCorners[i]);
            p = XMVectorMultiply(p, scale);
            p = XMVector3TransformCoord(p, rotationMatrix);
            p += center;

            XMVECTOR clip = XMVector3TransformCoord(p, viewProj);
            XMFLOAT3 ndc{};
            XMStoreFloat3(&ndc, clip);
            outCorners[i].x = viewRect.x + ((ndc.x * 0.5f) + 0.5f) * viewRect.z;
            outCorners[i].y = viewRect.y + ((-ndc.y * 0.5f) + 0.5f) * viewRect.w;
        }
        return true;
    }

    static UIHitTestResult PickTopmost(Registry& registry,
                                       const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection,
                                       const DirectX::XMFLOAT2& screenPoint)
    {
        using namespace DirectX;

        UIHitTestResult result;
        if (!ScreenToCanvasPoint(viewRect, view, projection, screenPoint, result.worldPoint)) {
            return result;
        }

        int bestSortingLayer = (std::numeric_limits<int>::min)();
        int bestOrder = (std::numeric_limits<int>::min)();
        float bestZ = -(std::numeric_limits<float>::max)();

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<RectTransformComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<CanvasItemComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
                continue;
            }

            auto* rectColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<RectTransformComponent>());
            auto* canvasColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<CanvasItemComponent>());
            auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
            auto* hierarchyColumn = signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<HierarchyComponent>())
                : nullptr;

            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                const EntityID entity = entities[i];
                const auto* rect = static_cast<RectTransformComponent*>(rectColumn->Get(i));
                const auto* canvas = static_cast<CanvasItemComponent*>(canvasColumn->Get(i));
                const auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
                const auto* hierarchy = hierarchyColumn
                    ? static_cast<HierarchyComponent*>(hierarchyColumn->Get(i))
                    : nullptr;

                if (!rect || !canvas || !transform || !canvas->visible || !canvas->interactable) {
                    continue;
                }
                if (hierarchy && !hierarchy->isActive) {
                    continue;
                }

                const XMFLOAT3 delta = {
                    result.worldPoint.x - transform->worldPosition.x,
                    result.worldPoint.y - transform->worldPosition.y,
                    0.0f
                };

                const XMVECTOR invRotation = XMQuaternionInverse(XMLoadFloat4(&transform->worldRotation));
                XMVECTOR local = XMVector3Rotate(XMLoadFloat3(&delta), invRotation);

                XMFLOAT3 localPoint{};
                XMStoreFloat3(&localPoint, local);

                const float scaleX = std::fabs(transform->worldScale.x) > 0.00001f ? std::fabs(transform->worldScale.x) : 1.0f;
                const float scaleY = std::fabs(transform->worldScale.y) > 0.00001f ? std::fabs(transform->worldScale.y) : 1.0f;

                const float normalizedX = localPoint.x / scaleX;
                const float normalizedY = localPoint.y / scaleY;

                const float minX = -rect->pivot.x * rect->sizeDelta.x;
                const float maxX = (1.0f - rect->pivot.x) * rect->sizeDelta.x;
                const float minY = -rect->pivot.y * rect->sizeDelta.y;
                const float maxY = (1.0f - rect->pivot.y) * rect->sizeDelta.y;

                if (normalizedX < minX || normalizedX > maxX || normalizedY < minY || normalizedY > maxY) {
                    continue;
                }

                const int sortingLayer = canvas->sortingLayer;
                const int order = canvas->orderInLayer;
                const float z = transform->worldPosition.z;
                if (sortingLayer > bestSortingLayer ||
                    (sortingLayer == bestSortingLayer && order > bestOrder) ||
                    (sortingLayer == bestSortingLayer && order == bestOrder && z > bestZ) ||
                    (sortingLayer == bestSortingLayer && order == bestOrder && std::fabs(z - bestZ) < 0.0001f && entity > result.entity)) {
                    bestSortingLayer = sortingLayer;
                    bestOrder = order;
                    bestZ = z;
                    result.entity = entity;
                }
            }
        }

        return result;
    }
};
