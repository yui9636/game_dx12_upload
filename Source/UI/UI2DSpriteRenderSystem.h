#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Component/RectTransformComponent.h"
#include "Component/TransformComponent.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "Sprite/Sprite.h"
#include "Sprite/SpriteRenderer.h"
#include "System/ResourceManager.h"
#include "UI/UIHitTestSystem.h"
#include "UI/UI2DLayoutResolver.h"

class UI2DSpriteRenderSystem
{
public:
    static void RenderSprites(const std::vector<UI2DSpritePacket>& packets,
                              const std::vector<UI2DLayoutNode>& layoutNodes,
                              const RenderContext& rc)
    {
        if (packets.empty() || !rc.commandList || rc.displayWidth <= 1 || rc.displayHeight <= 1) {
            return;
        }

        const DirectX::XMFLOAT4 viewRect{
            0.0f,
            0.0f,
            static_cast<float>(rc.displayWidth),
            static_cast<float>(rc.displayHeight)
        };
        UI2DLayoutResolver layoutResolver(layoutNodes, rc);

        for (const UI2DSpritePacket& packet : packets) {
            auto texture = ResourceManager::Instance().GetTexture(packet.textureAssetPath);
            if (!texture) {
                continue;
            }

            TransformComponent transform{};
            transform.worldPosition = packet.worldPosition;
            transform.worldRotation = packet.worldRotation;
            transform.worldScale = packet.worldScale;

            RectTransformComponent rect{};
            rect.sizeDelta = packet.sizeDelta;
            rect.pivot = packet.pivot;

            std::array<DirectX::XMFLOAT2, 4> corners{};
            bool pixelSnap = packet.pixelSnap;
            bool screenSpaceOverlay = packet.screenSpaceOverlay;
            if (!layoutResolver.ResolveCorners(packet.entity, corners, pixelSnap, screenSpaceOverlay)) {
                if (packet.screenSpaceOverlay) {
                    continue;
                }
                if (!UIHitTestSystem::ComputeScreenCorners(transform, rect, viewRect, rc.viewMatrix, rc.projectionMatrix, corners)) {
                    continue;
                }
            }

            if (pixelSnap || (screenSpaceOverlay && rc.cameraPixelSnap)) {
                for (auto& corner : corners) {
                    corner.x = std::round(corner.x);
                    corner.y = std::round(corner.y);
                }
            }

            if (packet.fillClipEnabled) {
                ApplyFillClip(packet.fillRatio, packet.fillDirection, corners);
                if (packet.fillRatio <= 0.0001f) {
                    continue;
                }
            }

            Sprite sprite;
            sprite.SetTexture(texture);
            if (packet.fillClipEnabled) {
                std::array<DirectX::XMFLOAT2, 4> uvs{};
                BuildFillUV(packet.fillRatio, packet.fillDirection, uvs);
                SpriteRenderer::Instance().DrawQuadUV(
                    sprite,
                    corners[0],
                    corners[1],
                    corners[2],
                    corners[3],
                    uvs[0],
                    uvs[1],
                    uvs[2],
                    uvs[3],
                    packet.tint);
            } else {
                SpriteRenderer::Instance().DrawQuad(
                    sprite,
                    corners[0],
                    corners[1],
                    corners[2],
                    corners[3],
                    packet.tint);
            }
        }
    }

private:
    static DirectX::XMFLOAT2 Lerp(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, float t)
    {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
    }

    static float Clamp01(float value)
    {
        return (std::max)(0.0f, (std::min)(1.0f, value));
    }

    static void ApplyFillClip(float ratio, int direction, std::array<DirectX::XMFLOAT2, 4>& corners)
    {
        ratio = Clamp01(ratio);
        if (ratio >= 0.9999f) {
            return;
        }

        const auto c0 = corners[0];
        const auto c1 = corners[1];
        const auto c2 = corners[2];
        const auto c3 = corners[3];

        switch (direction) {
        case 1: // 右から左へ塗る。
            corners[0] = Lerp(c1, c0, ratio);
            corners[1] = c1;
            corners[2] = c2;
            corners[3] = Lerp(c2, c3, ratio);
            break;
        case 2: // 下から上へ塗る。
            corners[0] = Lerp(c3, c0, ratio);
            corners[1] = Lerp(c2, c1, ratio);
            corners[2] = c2;
            corners[3] = c3;
            break;
        case 3: // 上から下へ塗る。
            corners[0] = c0;
            corners[1] = c1;
            corners[2] = Lerp(c1, c2, ratio);
            corners[3] = Lerp(c0, c3, ratio);
            break;
        case 0: // 左から右へ塗る。
        default:
            corners[0] = c0;
            corners[1] = Lerp(c0, c1, ratio);
            corners[2] = Lerp(c3, c2, ratio);
            corners[3] = c3;
            break;
        }
    }

    static void BuildFillUV(float ratio, int direction, std::array<DirectX::XMFLOAT2, 4>& uvs)
    {
        ratio = Clamp01(ratio);
        switch (direction) {
        case 1: // 右から左へ塗る。
            uvs[0] = { 1.0f - ratio, 0.0f };
            uvs[1] = { 1.0f, 0.0f };
            uvs[2] = { 1.0f, 1.0f };
            uvs[3] = { 1.0f - ratio, 1.0f };
            break;
        case 2: // 下から上へ塗る。
            uvs[0] = { 0.0f, 1.0f - ratio };
            uvs[1] = { 1.0f, 1.0f - ratio };
            uvs[2] = { 1.0f, 1.0f };
            uvs[3] = { 0.0f, 1.0f };
            break;
        case 3: // 上から下へ塗る。
            uvs[0] = { 0.0f, 0.0f };
            uvs[1] = { 1.0f, 0.0f };
            uvs[2] = { 1.0f, ratio };
            uvs[3] = { 0.0f, ratio };
            break;
        case 0: // 左から右へ塗る。
        default:
            uvs[0] = { 0.0f, 0.0f };
            uvs[1] = { ratio, 0.0f };
            uvs[2] = { ratio, 1.0f };
            uvs[3] = { 0.0f, 1.0f };
            break;
        }
    }
};
