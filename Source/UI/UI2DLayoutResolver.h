#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "Component/RectTransformComponent.h"
#include "Component/TransformComponent.h"
#include "Entity/Entity.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "UI/UIHitTestSystem.h"

class UI2DLayoutResolver
{
public:
    UI2DLayoutResolver(const std::vector<UI2DLayoutNode>& nodes, const RenderContext& rc)
        : m_nodes(nodes), m_rc(rc)
    {
        m_index.reserve(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            m_index[nodes[i].entity] = i;
        }
    }

    bool ResolveCorners(EntityID entity,
                        std::array<DirectX::XMFLOAT2, 4>& outCorners,
                        bool& outPixelSnap,
                        bool& outScreenSpaceOverlay)
    {
        const UI2DLayoutNode* node = FindNode(entity);
        if (!node) {
            return false;
        }

        outPixelSnap = node->pixelSnap;
        outScreenSpaceOverlay = node->screenSpaceOverlay;

        if (!node->screenSpaceOverlay) {
            return ResolveWorldCorners(*node, outCorners);
        }

        ScreenRect rect{};
        if (!ResolveScreenRect(entity, rect)) {
            return false;
        }

        BuildScreenCorners(rect, outCorners);
        return true;
    }

private:
    struct ScreenRect
    {
        bool resolved = false;
        bool resolving = false;
        bool valid = false;
        DirectX::XMFLOAT2 pivotPosition = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 size = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
        DirectX::XMFLOAT2 scale = { 1.0f, 1.0f };
        float rotationRad = 0.0f;
    };

    const UI2DLayoutNode* FindNode(EntityID entity) const
    {
        const auto it = m_index.find(entity);
        if (it == m_index.end() || it->second >= m_nodes.size()) {
            return nullptr;
        }
        return &m_nodes[it->second];
    }

    ScreenRect MakeRootScreenRect() const
    {
        const float width = (std::max)(1.0f, static_cast<float>(m_rc.displayWidth));
        const float height = (std::max)(1.0f, static_cast<float>(m_rc.displayHeight));

        ScreenRect rect{};
        rect.resolved = true;
        rect.valid = true;
        rect.pivotPosition = { 0.0f, 0.0f };
        rect.size = { width, height };
        rect.pivot = { 0.5f, 0.5f };
        rect.scale = { 1.0f, 1.0f };
        rect.rotationRad = 0.0f;
        return rect;
    }

    bool ResolveScreenRect(EntityID entity, ScreenRect& outRect)
    {
        auto cacheIt = m_cache.find(entity);
        if (cacheIt != m_cache.end()) {
            outRect = cacheIt->second;
            return outRect.valid;
        }

        ScreenRect pending{};
        pending.resolving = true;
        m_cache[entity] = pending;

        const UI2DLayoutNode* node = FindNode(entity);
        if (!node || !node->screenSpaceOverlay) {
            m_cache[entity] = ScreenRect{};
            outRect = m_cache[entity];
            return false;
        }

        ScreenRect parent = MakeRootScreenRect();
        if (!Entity::IsNull(node->parent)) {
            const UI2DLayoutNode* parentNode = FindNode(node->parent);
            if (parentNode && parentNode->screenSpaceOverlay) {
                ScreenRect resolvedParent{};
                auto parentCacheIt = m_cache.find(node->parent);
                const bool parentIsResolving = parentCacheIt != m_cache.end() && parentCacheIt->second.resolving;
                if (!parentIsResolving && ResolveScreenRect(node->parent, resolvedParent)) {
                    parent = resolvedParent;
                }
            }
        }

        const float parentMinX = parent.pivotPosition.x - parent.pivot.x * parent.size.x;
        const float parentMinY = parent.pivotPosition.y - parent.pivot.y * parent.size.y;
        const float parentWidth = (std::max)(0.0f, parent.size.x);
        const float parentHeight = (std::max)(0.0f, parent.size.y);

        const float anchorMinX = parentMinX + node->anchorMin.x * parentWidth;
        const float anchorMaxX = parentMinX + node->anchorMax.x * parentWidth;
        const float anchorMinY = parentMinY + node->anchorMin.y * parentHeight;
        const float anchorMaxY = parentMinY + node->anchorMax.y * parentHeight;

        const float anchorWidth = anchorMaxX - anchorMinX;
        const float anchorHeight = anchorMaxY - anchorMinY;

        ScreenRect rect{};
        rect.resolved = true;
        rect.valid = true;
        rect.size = {
            (std::max)(0.001f, anchorWidth + node->sizeDelta.x),
            (std::max)(0.001f, anchorHeight + node->sizeDelta.y)
        };
        rect.pivot = node->pivot;
        rect.scale = {
            parent.scale.x * ((std::fabs(node->scale2D.x) > 0.0001f) ? node->scale2D.x : 1.0f),
            parent.scale.y * ((std::fabs(node->scale2D.y) > 0.0001f) ? node->scale2D.y : 1.0f)
        };
        rect.rotationRad = parent.rotationRad + DirectX::XMConvertToRadians(node->rotationZ);
        rect.pivotPosition = {
            (anchorMinX + anchorMaxX) * 0.5f + node->anchoredPosition.x,
            (anchorMinY + anchorMaxY) * 0.5f + node->anchoredPosition.y
        };

        m_cache[entity] = rect;
        outRect = rect;
        return true;
    }

    bool ResolveWorldCorners(const UI2DLayoutNode& node, std::array<DirectX::XMFLOAT2, 4>& outCorners) const
    {
        const DirectX::XMFLOAT4 viewRect{
            0.0f,
            0.0f,
            static_cast<float>(m_rc.displayWidth),
            static_cast<float>(m_rc.displayHeight)
        };

        TransformComponent transform{};
        transform.worldPosition = node.worldPosition;
        transform.worldRotation = node.worldRotation;
        transform.worldScale = node.worldScale;

        RectTransformComponent rect{};
        rect.sizeDelta = node.sizeDelta;
        rect.pivot = node.pivot;

        return UIHitTestSystem::ComputeScreenCorners(
            transform,
            rect,
            viewRect,
            m_rc.viewMatrix,
            m_rc.projectionMatrix,
            outCorners);
    }

    void BuildScreenCorners(const ScreenRect& rect, std::array<DirectX::XMFLOAT2, 4>& outCorners) const
    {
        const float minX = -rect.pivot.x * rect.size.x;
        const float maxX = (1.0f - rect.pivot.x) * rect.size.x;
        const float minY = -rect.pivot.y * rect.size.y;
        const float maxY = (1.0f - rect.pivot.y) * rect.size.y;

        const std::array<DirectX::XMFLOAT2, 4> local = {
            DirectX::XMFLOAT2{ minX, minY },
            DirectX::XMFLOAT2{ maxX, minY },
            DirectX::XMFLOAT2{ maxX, maxY },
            DirectX::XMFLOAT2{ minX, maxY }
        };

        const float c = std::cos(rect.rotationRad);
        const float s = std::sin(rect.rotationRad);
        const float screenHalfW = static_cast<float>(m_rc.displayWidth) * 0.5f;
        const float screenHalfH = static_cast<float>(m_rc.displayHeight) * 0.5f;

        for (size_t i = 0; i < local.size(); ++i) {
            const float sx = local[i].x * rect.scale.x;
            const float sy = local[i].y * rect.scale.y;
            const float canvasX = rect.pivotPosition.x + sx * c - sy * s;
            const float canvasY = rect.pivotPosition.y + sx * s + sy * c;
            outCorners[i].x = screenHalfW + canvasX;
            outCorners[i].y = screenHalfH - canvasY;
        }
    }

    const std::vector<UI2DLayoutNode>& m_nodes;
    const RenderContext& m_rc;
    std::unordered_map<EntityID, size_t> m_index;
    std::unordered_map<EntityID, ScreenRect> m_cache;
};
