#pragma once

#include <filesystem>

#include "Entity/Entity.h"
#include "Undo/EntitySnapshot.h"

class Registry;

class PrefabSystem
{
public:
    static bool SaveEntityAsPrefab(EntityID root,
                                   Registry& registry,
                                   const std::filesystem::path& destinationDir,
                                   std::filesystem::path* outPath = nullptr);

    static EntityID InstantiatePrefab(const std::filesystem::path& prefabPath,
                                      Registry& registry,
                                      EntityID parentEntity = Entity::NULL_ID);

    static bool SaveEntityToPrefabPath(EntityID root,
                                       Registry& registry,
                                       const std::filesystem::path& prefabPath);
    static bool SaveRegistryAsScene(Registry& registry,
                                    const std::filesystem::path& scenePath);
    static bool LoadSceneIntoRegistry(const std::filesystem::path& scenePath,
                                      Registry& registry);

    static bool LoadPrefabSnapshot(const std::filesystem::path& prefabPath,
                                   EntitySnapshot::Snapshot& outSnapshot);

    static bool ApplyPrefab(EntityID root, Registry& registry);
    static EntityID RevertPrefab(EntityID root, Registry& registry);
    static bool UnpackPrefab(EntityID root, Registry& registry);

    static EntityID FindPrefabRoot(EntityID entity, Registry& registry);
    static void MarkPrefabOverride(EntityID entity, Registry& registry);
    static bool CanReparent(EntityID entity, EntityID newParent, Registry& registry);
    static bool CanCreateChild(EntityID parentEntity, Registry& registry);
    static bool CanDelete(EntityID entity, Registry& registry);
    static bool CanDuplicate(EntityID entity, Registry& registry);
};
