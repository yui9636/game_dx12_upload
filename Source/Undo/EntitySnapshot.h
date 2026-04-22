#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Component/AudioOneShotRequestComponent.h"
#include "Component/AudioStateComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/MeshComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/PhysicsComponent.h"
#include "Component/ReflectionProbeComponent.h"
#include "Entity/Entity.h"
#include "Generated/ComponentMeta.generated.h"
#include "Hierarchy/HierarchySystem.h"
#include "Registry/Registry.h"
#include "System/ResourceManager.h"

namespace EntitySnapshot
{
    static_assert(std::tuple_size_v<AllComponentTypes> <= MAX_COMPONENTS,
                  "MAX_COMPONENTS must be large enough for all generated component types.");

    constexpr uint32_t kInvalidLocalID = (std::numeric_limits<uint32_t>::max)();

    template<typename Tuple>
    struct OptionalTuple;

    template<typename... Ts>
    struct OptionalTuple<std::tuple<Ts...>> {
        using type = std::tuple<std::optional<Ts>...>;
    };

    using ComponentStorage = OptionalTuple<AllComponentTypes>::type;

    struct Node
    {
        uint32_t localID = kInvalidLocalID;
        EntityID sourceEntity = Entity::NULL_ID;
        uint32_t parentLocalID = kInvalidLocalID;
        EntityID externalParent = Entity::NULL_ID;
        ComponentStorage components;
    };

    struct Snapshot
    {
        uint32_t rootLocalID = kInvalidLocalID;
        std::vector<Node> nodes;
    };

    struct RestoreResult
    {
        EntityID root = Entity::NULL_ID;
        std::unordered_map<uint32_t, EntityID> localToEntity;
    };

    inline void CollectHierarchy(EntityID entity, Registry& registry, std::vector<EntityID>& outEntities)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return;
        }

        outEntities.push_back(entity);

        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            EntityID child = hierarchy->firstChild;
            while (!Entity::IsNull(child)) {
                EntityID next = Entity::NULL_ID;
                if (auto* childHierarchy = registry.GetComponent<HierarchyComponent>(child)) {
                    next = childHierarchy->nextSibling;
                }
                CollectHierarchy(child, registry, outEntities);
                child = next;
            }
        }
    }

    template<typename T>
    inline void CaptureComponent(EntityID entity, Registry& registry, ComponentStorage& storage)
    {
        if (auto* component = registry.GetComponent<T>(entity)) {
            std::get<std::optional<T>>(storage) = *component;
        }
    }

    template<typename T>
    inline void RestoreComponent(EntityID entity, Registry& registry, const ComponentStorage& storage)
    {
        const auto& value = std::get<std::optional<T>>(storage);
        if (value.has_value()) {
            registry.AddComponent<T>(entity, *value);
        }
    }

    template<>
    inline void CaptureComponent<AudioOneShotRequestComponent>(EntityID, Registry&, ComponentStorage&)
    {
    }

    template<>
    inline void RestoreComponent<AudioOneShotRequestComponent>(EntityID, Registry&, const ComponentStorage&)
    {
    }

    template<>
    inline void CaptureComponent<AudioStateComponent>(EntityID, Registry&, ComponentStorage&)
    {
    }

    template<>
    inline void RestoreComponent<AudioStateComponent>(EntityID, Registry&, const ComponentStorage&)
    {
    }

    inline void CaptureAllComponents(EntityID entity, Registry& registry, ComponentStorage& storage)
    {
        std::apply(
            [&](auto... component) {
                (CaptureComponent<std::decay_t<decltype(component)>>(entity, registry, storage), ...);
            },
            AllComponentTypes{});
    }

    inline void RestoreAllComponents(EntityID entity, Registry& registry, const ComponentStorage& storage)
    {
        std::apply(
            [&](auto... component) {
                (RestoreComponent<std::decay_t<decltype(component)>>(entity, registry, storage), ...);
            },
            AllComponentTypes{});
    }

    inline Snapshot CaptureSubtree(EntityID root, Registry& registry)
    {
        Snapshot snapshot;
        if (Entity::IsNull(root) || !registry.IsAlive(root)) {
            return snapshot;
        }

        std::vector<EntityID> entities;
        CollectHierarchy(root, registry, entities);
        if (entities.empty()) {
            return snapshot;
        }

        std::unordered_map<EntityID, uint32_t> oldToLocal;
        snapshot.nodes.reserve(entities.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(entities.size()); ++i) {
            oldToLocal[entities[i]] = i;
        }

        snapshot.rootLocalID = oldToLocal[root];

        for (EntityID entity : entities) {
            Node node;
            node.localID = oldToLocal[entity];
            node.sourceEntity = entity;

            EntityID parent = Entity::NULL_ID;
            if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
                parent = hierarchy->parent;
            } else if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                parent = (transform->parent == 0) ? Entity::NULL_ID : transform->parent;
            }

            const auto it = oldToLocal.find(parent);
            if (it != oldToLocal.end()) {
                node.parentLocalID = it->second;
            } else {
                node.externalParent = parent;
            }

            CaptureAllComponents(entity, registry, node.components);
            snapshot.nodes.push_back(std::move(node));
        }

        return snapshot;
    }

    inline EntityID RemapEntityReference(EntityID original,
                                         const std::unordered_map<EntityID, uint32_t>& sourceToLocal,
                                         const std::unordered_map<uint32_t, EntityID>& localToEntity)
    {
        if (Entity::IsNull(original)) {
            return Entity::NULL_ID;
        }

        const auto localIt = sourceToLocal.find(original);
        if (localIt == sourceToLocal.end()) {
            return original;
        }

        const auto newIt = localToEntity.find(localIt->second);
        return (newIt != localToEntity.end()) ? newIt->second : Entity::NULL_ID;
    }

    inline void SanitizeRuntimeState(EntityID entity,
                                     Registry& registry,
                                     const std::unordered_map<EntityID, uint32_t>& sourceToLocal,
                                     const std::unordered_map<uint32_t, EntityID>& localToEntity)
    {
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            transform->parent = 0;
            transform->isDirty = true;
        }

        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            hierarchy->parent = Entity::NULL_ID;
            hierarchy->firstChild = Entity::NULL_ID;
            hierarchy->prevSibling = Entity::NULL_ID;
            hierarchy->nextSibling = Entity::NULL_ID;
        }

        if (auto* tpc = registry.GetComponent<CameraTPVControlComponent>(entity)) {
            tpc->target = RemapEntityReference(tpc->target, sourceToLocal, localToEntity);
        }

        if (auto* lookAt = registry.GetComponent<CameraLookAtComponent>(entity)) {
            lookAt->target = RemapEntityReference(lookAt->target, sourceToLocal, localToEntity);
        }

        if (auto* collider = registry.GetComponent<ColliderComponent>(entity)) {
            for (auto& element : collider->elements) {
                element.registeredId = 0;
                element.runtimeTag = 0;
            }
        }

        if (auto* physics = registry.GetComponent<PhysicsComponent>(entity)) {
            physics->bodyID = JPH::BodyID();
        }

        if (auto* material = registry.GetComponent<MaterialComponent>(entity)) {
            if (!material->materialAssetPath.empty()) {
                material->materialAsset = ResourceManager::Instance().GetMaterial(material->materialAssetPath);
            }
        }

        if (auto* mesh = registry.GetComponent<MeshComponent>(entity)) {
            if (!mesh->modelFilePath.empty()) {
                // Each ECS entity needs its own mutable model state for transform/animation updates.
                mesh->model = ResourceManager::Instance().CreateModelInstance(mesh->modelFilePath);
            }
        }

        if (auto* probe = registry.GetComponent<ReflectionProbeComponent>(entity)) {
            probe->cubemapSRV.Reset();
            probe->cubemapTexture.reset();
            probe->needsBake = true;
        }

        if (auto* effectPlayback = registry.GetComponent<EffectPlaybackComponent>(entity)) {
            effectPlayback->isPlaying = false;
            effectPlayback->currentTime = 0.0f;
            effectPlayback->runtimeInstanceId = 0;
            effectPlayback->stopRequested = false;
            effectPlayback->lifetimeFade = 1.0f;
        }
    }

    inline RestoreResult RestoreSubtree(const Snapshot& snapshot, Registry& registry)
    {
        RestoreResult result;
        if (snapshot.nodes.empty()) {
            return result;
        }

        std::unordered_map<EntityID, uint32_t> sourceToLocal;
        for (const Node& node : snapshot.nodes) {
            sourceToLocal[node.sourceEntity] = node.localID;
            result.localToEntity[node.localID] = registry.CreateEntity();
        }

        for (const Node& node : snapshot.nodes) {
            const EntityID entity = result.localToEntity[node.localID];
            RestoreAllComponents(entity, registry, node.components);
            SanitizeRuntimeState(entity, registry, sourceToLocal, result.localToEntity);
        }

        for (const Node& node : snapshot.nodes) {
            const EntityID entity = result.localToEntity[node.localID];
            EntityID parent = Entity::NULL_ID;

            if (node.parentLocalID != kInvalidLocalID) {
                const auto it = result.localToEntity.find(node.parentLocalID);
                if (it != result.localToEntity.end()) {
                    parent = it->second;
                }
            } else if (!Entity::IsNull(node.externalParent) && registry.IsAlive(node.externalParent)) {
                parent = node.externalParent;
            }

            HierarchySystem::Reparent(entity, parent, registry, false);
        }

        const auto rootIt = result.localToEntity.find(snapshot.rootLocalID);
        if (rootIt != result.localToEntity.end()) {
            result.root = rootIt->second;
        }

        return result;
    }

    inline void DestroySubtree(EntityID root, Registry& registry)
    {
        if (Entity::IsNull(root) || !registry.IsAlive(root)) {
            return;
        }

        std::vector<EntityID> entities;
        CollectHierarchy(root, registry, entities);
        for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
            if (registry.IsAlive(*it)) {
                registry.DestroyEntity(*it);
            }
        }
    }

    inline void AppendRootNameSuffix(Snapshot& snapshot, const std::string& suffix)
    {
        for (Node& node : snapshot.nodes) {
            if (node.localID != snapshot.rootLocalID) {
                continue;
            }

            auto& nameComponent = std::get<std::optional<NameComponent>>(node.components);
            if (!nameComponent.has_value()) {
                nameComponent = NameComponent{};
            }

            auto stripSuffix = [&](const std::string& candidateSuffix) {
                while (nameComponent->name.size() >= candidateSuffix.size() &&
                       nameComponent->name.compare(nameComponent->name.size() - candidateSuffix.size(),
                                                  candidateSuffix.size(),
                                                  candidateSuffix) == 0) {
                    nameComponent->name.erase(nameComponent->name.size() - candidateSuffix.size());
                }
            };

            stripSuffix(" (Clone)");
            stripSuffix(" (Copy)");
            nameComponent->name += suffix;
            break;
        }
    }
}
