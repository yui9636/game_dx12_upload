#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Entity/Entity.h"
#include "Registry/Registry.h"

struct UI2DDrawEntry
{
    EntityID entity = Entity::NULL_ID;
    NameComponent* name = nullptr;
    HierarchyComponent* hierarchy = nullptr;
    TransformComponent* transform = nullptr;
    RectTransformComponent* rect = nullptr;
    CanvasItemComponent* canvas = nullptr;
    SpriteComponent* sprite = nullptr;
    TextComponent* text = nullptr;
    int sortingLayer = 0;
    int orderInLayer = 0;
    float z = 0.0f;
};

class UI2DDrawSystem
{
public:
    static std::vector<UI2DDrawEntry> CollectDrawEntries(Registry& registry)
    {
        std::vector<UI2DDrawEntry> entries;

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
            auto* nameColumn = signature.test(TypeManager::GetComponentTypeID<NameComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<NameComponent>())
                : nullptr;
            auto* hierarchyColumn = signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<HierarchyComponent>())
                : nullptr;
            auto* spriteColumn = signature.test(TypeManager::GetComponentTypeID<SpriteComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<SpriteComponent>())
                : nullptr;
            auto* textColumn = signature.test(TypeManager::GetComponentTypeID<TextComponent>())
                ? archetype->GetColumn(TypeManager::GetComponentTypeID<TextComponent>())
                : nullptr;

            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                auto* canvas = static_cast<CanvasItemComponent*>(canvasColumn->Get(i));
                auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
                auto* rect = static_cast<RectTransformComponent*>(rectColumn->Get(i));
                auto* hierarchy = hierarchyColumn ? static_cast<HierarchyComponent*>(hierarchyColumn->Get(i)) : nullptr;
                if (!canvas || !transform || !rect || !canvas->visible) {
                    continue;
                }
                if (hierarchy && !hierarchy->isActive) {
                    continue;
                }

                UI2DDrawEntry entry;
                entry.entity = entities[i];
                entry.name = nameColumn ? static_cast<NameComponent*>(nameColumn->Get(i)) : nullptr;
                entry.hierarchy = hierarchy;
                entry.transform = transform;
                entry.rect = rect;
                entry.canvas = canvas;
                entry.sprite = spriteColumn ? static_cast<SpriteComponent*>(spriteColumn->Get(i)) : nullptr;
                entry.text = textColumn ? static_cast<TextComponent*>(textColumn->Get(i)) : nullptr;
                entry.sortingLayer = canvas->sortingLayer;
                entry.orderInLayer = canvas->orderInLayer;
                entry.z = transform->worldPosition.z;
                entries.push_back(entry);
            }
        }

        std::sort(entries.begin(), entries.end(), [](const UI2DDrawEntry& a, const UI2DDrawEntry& b) {
            if (a.sortingLayer != b.sortingLayer) return a.sortingLayer < b.sortingLayer;
            if (a.orderInLayer != b.orderInLayer) return a.orderInLayer < b.orderInLayer;
            if (std::fabs(a.z - b.z) > 0.0001f) return a.z < b.z;
            return a.entity < b.entity;
        });
        return entries;
    }
};
