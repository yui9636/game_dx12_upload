#pragma once

#include <utility>

#include "RenderContext/RenderQueue.h"
#include "UI/UI2DDrawSystem.h"

class UI2DSpriteExtractSystem
{
public:
    static void Extract(Registry& registry, RenderQueue& queue)
    {
        const std::vector<UI2DDrawEntry> entries = UI2DDrawSystem::CollectDrawEntries(registry);
        for (const UI2DDrawEntry& entry : entries) {
            if (!entry.sprite || entry.sprite->textureAssetPath.empty() || !entry.transform || !entry.rect || !entry.canvas) {
                continue;
            }

            UI2DSpritePacket packet;
            packet.entity = entry.entity;
            packet.textureAssetPath = entry.sprite->textureAssetPath;
            packet.tint = entry.sprite->tint;
            packet.worldPosition = entry.transform->worldPosition;
            packet.worldRotation = entry.transform->worldRotation;
            packet.worldScale = entry.transform->worldScale;
            packet.sizeDelta = entry.rect->sizeDelta;
            packet.pivot = entry.rect->pivot;
            packet.pixelSnap = entry.canvas->pixelSnap;
            queue.ui2DSpritePackets.push_back(std::move(packet));
        }
    }
};
