// UI2DTextExtractSystem の UI 関連宣言をまとめます。
#pragma once

#include <utility>

#include "RenderContext/RenderQueue.h"
#include "UI/UI2DDrawSystem.h"

class UI2DTextExtractSystem
{
public:
    static void Extract(Registry& registry, RenderQueue& queue)
    {
        const std::vector<UI2DDrawEntry> entries = UI2DDrawSystem::CollectDrawEntries(registry);
        for (const UI2DDrawEntry& entry : entries) {
            if (!entry.text || entry.text->text.empty() || !entry.transform || !entry.rect || !entry.canvas) {
                continue;
            }

            UI2DTextPacket packet;
            packet.entity = entry.entity;
            packet.text = entry.text->text;
            packet.fontAssetPath = entry.text->fontAssetPath;
            packet.fontSize = entry.text->fontSize;
            packet.color = entry.text->color;
            packet.alignment = static_cast<int>(entry.text->alignment);
            packet.worldPosition = entry.transform->worldPosition;
            packet.worldRotation = entry.transform->worldRotation;
            packet.worldScale = entry.transform->worldScale;
            packet.sizeDelta = entry.rect->sizeDelta;
            packet.pivot = entry.rect->pivot;
            packet.pixelSnap = entry.canvas->pixelSnap;
            queue.ui2DTextPackets.push_back(std::move(packet));
        }
    }
};
