// UI2DSpriteExtractSystem の UI 関連宣言をまとめます。
#pragma once

#include <utility>

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/HPGaugeComponent.h"
#include "RenderContext/RenderQueue.h"
#include "UI/UI2DDrawSystem.h"

class UI2DSpriteExtractSystem
{
public:
    static void Extract(Registry& registry, RenderQueue& queue)
    {
        const std::vector<UI2DDrawEntry> entries = UI2DDrawSystem::CollectDrawEntries(registry);
        UI2DDrawSystem::AppendLayoutNodes(entries, queue);
        AppendWorldSpritePackets(registry, queue);
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
            packet.screenSpaceOverlay = entry.canvas->screenSpaceOverlay;
            packet.pixelSnap = entry.canvas->pixelSnap;
            if (auto* fill = registry.GetComponent<HPGaugeFillComponent>(entry.entity)) {
                packet.fillClipEnabled = true;
                packet.fillRatio = fill->runtimeRatio;
                packet.fillDirection = static_cast<int>(fill->fillDirection);
            }
            queue.ui2DSpritePackets.push_back(std::move(packet));
        }
    }

private:
    static void AppendWorldSpritePackets(Registry& registry, RenderQueue& queue)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            const bool hasSprite = signature.test(TypeManager::GetComponentTypeID<SpriteComponent>());
            const bool hasTransform = signature.test(TypeManager::GetComponentTypeID<TransformComponent>());
            const bool hasCanvas = signature.test(TypeManager::GetComponentTypeID<CanvasItemComponent>());
            const bool hasRect = signature.test(TypeManager::GetComponentTypeID<RectTransformComponent>());
            if (!hasSprite || !hasTransform) {
                continue;
            }
            if (hasCanvas && hasRect) {
                continue;
            }

            auto* spriteColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<SpriteComponent>());
            auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
            auto* rectColumn = hasRect ? archetype->GetColumn(TypeManager::GetComponentTypeID<RectTransformComponent>()) : nullptr;
            auto* canvasColumn = hasCanvas ? archetype->GetColumn(TypeManager::GetComponentTypeID<CanvasItemComponent>()) : nullptr;
            auto* hierarchyColumn = signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<HierarchyComponent>())
                : nullptr;

            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                const auto* sprite = static_cast<const SpriteComponent*>(spriteColumn->Get(i));
                const auto* transform = static_cast<const TransformComponent*>(transformColumn->Get(i));
                const auto* rect = rectColumn ? static_cast<const RectTransformComponent*>(rectColumn->Get(i)) : nullptr;
                const auto* canvas = canvasColumn ? static_cast<const CanvasItemComponent*>(canvasColumn->Get(i)) : nullptr;
                const auto* hierarchy = hierarchyColumn ? static_cast<const HierarchyComponent*>(hierarchyColumn->Get(i)) : nullptr;

                if (!sprite || sprite->textureAssetPath.empty() || !transform) {
                    continue;
                }
                if (hierarchy && !hierarchy->isActive) {
                    continue;
                }
                if (canvas && !canvas->visible) {
                    continue;
                }

                UI2DSpritePacket packet;
                packet.entity = entities[i];
                packet.textureAssetPath = sprite->textureAssetPath;
                packet.tint = sprite->tint;
                packet.worldPosition = transform->worldPosition;
                packet.worldRotation = transform->worldRotation;
                packet.worldScale = transform->worldScale;
                packet.sizeDelta = rect ? rect->sizeDelta : DirectX::XMFLOAT2{ 1.0f, 1.0f };
                packet.pivot = rect ? rect->pivot : DirectX::XMFLOAT2{ 0.5f, 0.5f };
                packet.screenSpaceOverlay = false;
                packet.pixelSnap = canvas ? canvas->pixelSnap : false;
                queue.ui2DSpritePackets.push_back(std::move(packet));
            }
        }
    }
};
