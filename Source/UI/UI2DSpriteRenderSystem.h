#pragma once

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

class UI2DSpriteRenderSystem
{
public:
    static void RenderSprites(const std::vector<UI2DSpritePacket>& packets, const RenderContext& rc)
    {
        if (packets.empty() || !rc.commandList || rc.displayWidth <= 1 || rc.displayHeight <= 1) {
            return;
        }
        if (std::fabs(rc.projectionMatrix._34) > 0.0001f) {
            return;
        }

        const DirectX::XMFLOAT4 viewRect{
            0.0f,
            0.0f,
            static_cast<float>(rc.displayWidth),
            static_cast<float>(rc.displayHeight)
        };

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
            if (!UIHitTestSystem::ComputeScreenCorners(transform, rect, viewRect, rc.viewMatrix, rc.projectionMatrix, corners)) {
                continue;
            }

            if (packet.pixelSnap || rc.cameraPixelSnap) {
                for (auto& corner : corners) {
                    corner.x = std::round(corner.x);
                    corner.y = std::round(corner.y);
                }
            }

            Sprite sprite;
            sprite.SetTexture(texture);
            SpriteRenderer::Instance().DrawQuad(
                sprite,
                corners[0],
                corners[1],
                corners[2],
                corners[3],
                packet.tint);
        }
    }
};
