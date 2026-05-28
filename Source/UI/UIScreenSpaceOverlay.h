#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include <DirectXMath.h>

#include "Entity/Entity.h"
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "UI/UI2DDrawSystem.h"
#include "UI/UI2DLayoutResolver.h"
#include "UI/UIHitTestSystem.h"

namespace UIScreenSpaceOverlay
{
    struct Bounds
    {
        std::array<DirectX::XMFLOAT2, 4> corners{};
        DirectX::XMFLOAT2 center{ 0.0f, 0.0f };
        DirectX::XMFLOAT2 min{ 0.0f, 0.0f };
        DirectX::XMFLOAT2 max{ 0.0f, 0.0f };
        bool visible = false;
        bool fullyVisible = false;
    };

    inline RenderContext MakeLayoutContext(const DirectX::XMFLOAT4& viewRect,
                                           const DirectX::XMFLOAT4X4& view,
                                           const DirectX::XMFLOAT4X4& projection)
    {
        RenderContext rc{};
        rc.displayWidth = static_cast<uint32_t>((std::max)(viewRect.z, 1.0f));
        rc.displayHeight = static_cast<uint32_t>((std::max)(viewRect.w, 1.0f));
        rc.viewMatrix = view;
        rc.projectionMatrix = projection;
        return rc;
    }

    inline void OffsetCornersToViewRect(std::array<DirectX::XMFLOAT2, 4>& corners,
                                        const DirectX::XMFLOAT4& viewRect)
    {
        for (auto& corner : corners) {
            corner.x += viewRect.x;
            corner.y += viewRect.y;
        }
    }

    inline bool PointInQuad(const std::array<DirectX::XMFLOAT2, 4>& corners,
                            const DirectX::XMFLOAT2& point)
    {
        bool hasNegative = false;
        bool hasPositive = false;
        for (size_t i = 0; i < corners.size(); ++i) {
            const DirectX::XMFLOAT2& a = corners[i];
            const DirectX::XMFLOAT2& b = corners[(i + 1) % corners.size()];
            const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
            hasNegative = hasNegative || cross < -0.001f;
            hasPositive = hasPositive || cross > 0.001f;
            if (hasNegative && hasPositive) {
                return false;
            }
        }
        return true;
    }

    inline bool ResolveCorners(Registry& registry,
                               EntityID entity,
                               const DirectX::XMFLOAT4& viewRect,
                               const DirectX::XMFLOAT4X4& view,
                               const DirectX::XMFLOAT4X4& projection,
                               std::array<DirectX::XMFLOAT2, 4>& outCorners)
    {
        const auto* canvas = registry.GetComponent<CanvasItemComponent>(entity);
        if (!canvas || !canvas->visible || !canvas->screenSpaceOverlay) {
            return false;
        }

        const std::vector<UI2DDrawEntry> entries = UI2DDrawSystem::CollectDrawEntries(registry);
        RenderQueue layoutQueue;
        UI2DDrawSystem::AppendLayoutNodes(entries, layoutQueue);
        RenderContext layoutRc = MakeLayoutContext(viewRect, view, projection);
        UI2DLayoutResolver resolver(layoutQueue.ui2DLayoutNodes, layoutRc);

        bool pixelSnap = false;
        bool screenSpaceOverlay = false;
        if (!resolver.ResolveCorners(entity, outCorners, pixelSnap, screenSpaceOverlay) || !screenSpaceOverlay) {
            return false;
        }

        if (pixelSnap) {
            for (auto& corner : outCorners) {
                corner.x = std::round(corner.x);
                corner.y = std::round(corner.y);
            }
        }
        OffsetCornersToViewRect(outCorners, viewRect);
        return true;
    }

    inline bool ResolveBounds(Registry& registry,
                              EntityID entity,
                              const DirectX::XMFLOAT4& viewRect,
                              const DirectX::XMFLOAT4X4& view,
                              const DirectX::XMFLOAT4X4& projection,
                              Bounds& outBounds)
    {
        if (!ResolveCorners(registry, entity, viewRect, view, projection, outBounds.corners)) {
            return false;
        }

        float minX = (std::numeric_limits<float>::max)();
        float minY = (std::numeric_limits<float>::max)();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (const auto& corner : outBounds.corners) {
            minX = (std::min)(minX, corner.x);
            minY = (std::min)(minY, corner.y);
            maxX = (std::max)(maxX, corner.x);
            maxY = (std::max)(maxY, corner.y);
        }

        outBounds.min = { minX, minY };
        outBounds.max = { maxX, maxY };
        outBounds.center = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f };

        const float right = viewRect.x + viewRect.z;
        const float bottom = viewRect.y + viewRect.w;
        outBounds.visible = maxX >= viewRect.x && minX <= right && maxY >= viewRect.y && minY <= bottom;
        outBounds.fullyVisible = minX >= viewRect.x && maxX <= right && minY >= viewRect.y && maxY <= bottom;
        return true;
    }

    inline UIHitTestResult PickTopmost(Registry& registry,
                                       const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection,
                                       const DirectX::XMFLOAT2& screenPoint)
    {
        UIHitTestResult result;
        if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
            return result;
        }
        if (screenPoint.x < viewRect.x || screenPoint.x > viewRect.x + viewRect.z ||
            screenPoint.y < viewRect.y || screenPoint.y > viewRect.y + viewRect.w) {
            return result;
        }

        const std::vector<UI2DDrawEntry> entries = UI2DDrawSystem::CollectDrawEntries(registry);
        RenderQueue layoutQueue;
        UI2DDrawSystem::AppendLayoutNodes(entries, layoutQueue);
        RenderContext layoutRc = MakeLayoutContext(viewRect, view, projection);
        UI2DLayoutResolver resolver(layoutQueue.ui2DLayoutNodes, layoutRc);

        int bestSortingLayer = (std::numeric_limits<int>::min)();
        int bestOrder = (std::numeric_limits<int>::min)();
        float bestZ = -(std::numeric_limits<float>::max)();

        for (const UI2DDrawEntry& entry : entries) {
            if (!entry.canvas || !entry.canvas->visible || !entry.canvas->interactable || !entry.canvas->screenSpaceOverlay) {
                continue;
            }

            std::array<DirectX::XMFLOAT2, 4> corners{};
            bool pixelSnap = false;
            bool screenSpaceOverlay = false;
            if (!resolver.ResolveCorners(entry.entity, corners, pixelSnap, screenSpaceOverlay) || !screenSpaceOverlay) {
                continue;
            }
            if (pixelSnap) {
                for (auto& corner : corners) {
                    corner.x = std::round(corner.x);
                    corner.y = std::round(corner.y);
                }
            }
            OffsetCornersToViewRect(corners, viewRect);
            if (!PointInQuad(corners, screenPoint)) {
                continue;
            }

            const int sortingLayer = entry.canvas->sortingLayer;
            const int order = entry.canvas->orderInLayer;
            const float z = entry.transform ? entry.transform->worldPosition.z : 0.0f;
            if (sortingLayer > bestSortingLayer ||
                (sortingLayer == bestSortingLayer && order > bestOrder) ||
                (sortingLayer == bestSortingLayer && order == bestOrder && z > bestZ) ||
                (sortingLayer == bestSortingLayer && order == bestOrder && std::fabs(z - bestZ) < 0.0001f && entry.entity > result.entity)) {
                bestSortingLayer = sortingLayer;
                bestOrder = order;
                bestZ = z;
                result.entity = entry.entity;
                result.worldPoint = { screenPoint.x, screenPoint.y, 0.0f };
            }
        }

        return result;
    }
}
