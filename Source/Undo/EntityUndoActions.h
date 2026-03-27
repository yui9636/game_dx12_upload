#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "Hierarchy/HierarchySystem.h"
#include "Component/PrefabInstanceComponent.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/IUndoAction.h"

class DuplicateEntityAction : public IUndoAction
{
public:
    DuplicateEntityAction(EntitySnapshot::Snapshot snapshot, EntityID parentEntity)
        : m_snapshot(std::move(snapshot))
        , m_parentEntity(parentEntity)
    {
    }

    void Undo(Registry& registry) override
    {
        EntitySnapshot::DestroySubtree(m_liveRoot, registry);
        m_liveRoot = Entity::NULL_ID;
    }

    void Redo(Registry& registry) override
    {
        if (!m_snapshot.nodes.empty()) {
            for (auto& node : m_snapshot.nodes) {
                if (node.localID == m_snapshot.rootLocalID) {
                    node.externalParent = m_parentEntity;
                    break;
                }
            }
        }

        EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(m_snapshot, registry);
        m_liveRoot = restore.root;
    }

    const char* GetName() const override { return "Duplicate Entity"; }

    EntityID GetLiveRoot() const { return m_liveRoot; }

private:
    EntitySnapshot::Snapshot m_snapshot;
    EntityID m_parentEntity = Entity::NULL_ID;
    EntityID m_liveRoot = Entity::NULL_ID;
};

class CreateEntityAction : public IUndoAction
{
public:
    CreateEntityAction(EntitySnapshot::Snapshot snapshot,
                       EntityID parentEntity,
                       std::string actionName = "Create Entity")
        : m_snapshot(std::move(snapshot))
        , m_parentEntity(parentEntity)
        , m_actionName(std::move(actionName))
    {
    }

    void Undo(Registry& registry) override
    {
        EntitySnapshot::DestroySubtree(m_liveRoot, registry);
        m_liveRoot = Entity::NULL_ID;
    }

    void Redo(Registry& registry) override
    {
        if (!m_snapshot.nodes.empty()) {
            for (auto& node : m_snapshot.nodes) {
                if (node.localID == m_snapshot.rootLocalID) {
                    node.externalParent = m_parentEntity;
                    break;
                }
            }
        }

        EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(m_snapshot, registry);
        m_liveRoot = restore.root;
    }

    const char* GetName() const override { return m_actionName.c_str(); }
    EntityID GetLiveRoot() const { return m_liveRoot; }
    void AdoptLiveRoot(EntityID liveRoot) { m_liveRoot = liveRoot; }

private:
    EntitySnapshot::Snapshot m_snapshot;
    EntityID m_parentEntity = Entity::NULL_ID;
    std::string m_actionName;
    EntityID m_liveRoot = Entity::NULL_ID;
};

class DeleteEntityAction : public IUndoAction
{
public:
    DeleteEntityAction(EntitySnapshot::Snapshot snapshot, EntityID liveRoot)
        : m_snapshot(std::move(snapshot))
        , m_liveRoot(liveRoot)
    {
    }

    void Undo(Registry& registry) override
    {
        EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(m_snapshot, registry);
        m_liveRoot = restore.root;
    }

    void Redo(Registry& registry) override
    {
        EntitySnapshot::DestroySubtree(m_liveRoot, registry);
        m_liveRoot = Entity::NULL_ID;
    }

    const char* GetName() const override { return "Delete Entity"; }

    EntityID GetLiveRoot() const { return m_liveRoot; }

private:
    EntitySnapshot::Snapshot m_snapshot;
    EntityID m_liveRoot = Entity::NULL_ID;
};

class ReparentEntityAction : public IUndoAction
{
public:
    ReparentEntityAction(EntityID entity, EntityID newParent, EntityID oldParent, bool keepWorldTransform)
        : m_entity(entity)
        , m_newParent(newParent)
        , m_oldParent(oldParent)
        , m_keepWorldTransform(keepWorldTransform)
    {
    }

    void Undo(Registry& registry) override
    {
        if (registry.IsAlive(m_entity)) {
            HierarchySystem::Reparent(m_entity, m_oldParent, registry, m_keepWorldTransform);
        }
    }

    void Redo(Registry& registry) override
    {
        if (registry.IsAlive(m_entity)) {
            HierarchySystem::Reparent(m_entity, m_newParent, registry, m_keepWorldTransform);
        }
    }

    const char* GetName() const override { return "Reparent Entity"; }

private:
    EntityID m_entity = Entity::NULL_ID;
    EntityID m_newParent = Entity::NULL_ID;
    EntityID m_oldParent = Entity::NULL_ID;
    bool m_keepWorldTransform = true;
};

class ReplaceEntitySubtreeAction : public IUndoAction
{
public:
    ReplaceEntitySubtreeAction(EntitySnapshot::Snapshot beforeSnapshot,
                               EntitySnapshot::Snapshot afterSnapshot,
                               EntityID liveRoot,
                               EntityID parentEntity,
                               std::string actionName)
        : m_beforeSnapshot(std::move(beforeSnapshot))
        , m_afterSnapshot(std::move(afterSnapshot))
        , m_liveRoot(liveRoot)
        , m_parentEntity(parentEntity)
        , m_actionName(std::move(actionName))
    {
    }

    void Undo(Registry& registry) override
    {
        ApplySnapshot(m_beforeSnapshot, registry);
    }

    void Redo(Registry& registry) override
    {
        ApplySnapshot(m_afterSnapshot, registry);
    }

    const char* GetName() const override { return m_actionName.c_str(); }
    EntityID GetLiveRoot() const { return m_liveRoot; }

private:
    void ApplySnapshot(const EntitySnapshot::Snapshot& snapshot, Registry& registry)
    {
        if (!Entity::IsNull(m_liveRoot) && registry.IsAlive(m_liveRoot)) {
            EntitySnapshot::DestroySubtree(m_liveRoot, registry);
        }

        EntitySnapshot::Snapshot restoreSnapshot = snapshot;
        for (auto& node : restoreSnapshot.nodes) {
            if (node.localID == restoreSnapshot.rootLocalID) {
                node.externalParent = m_parentEntity;
                break;
            }
        }

        EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(restoreSnapshot, registry);
        m_liveRoot = restore.root;
    }

    EntitySnapshot::Snapshot m_beforeSnapshot;
    EntitySnapshot::Snapshot m_afterSnapshot;
    EntityID m_liveRoot = Entity::NULL_ID;
    EntityID m_parentEntity = Entity::NULL_ID;
    std::string m_actionName;
};

class ApplyPrefabAction : public IUndoAction
{
public:
    ApplyPrefabAction(EntityID root,
                      std::filesystem::path prefabPath,
                      std::string beforeText,
                      std::string afterText,
                      bool oldHasOverrides,
                      bool newHasOverrides)
        : m_root(root)
        , m_prefabPath(std::move(prefabPath))
        , m_beforeText(std::move(beforeText))
        , m_afterText(std::move(afterText))
        , m_oldHasOverrides(oldHasOverrides)
        , m_newHasOverrides(newHasOverrides)
    {
    }

    void Undo(Registry& registry) override
    {
        WriteText(m_beforeText);
        if (auto* prefab = registry.GetComponent<PrefabInstanceComponent>(m_root)) {
            prefab->hasOverrides = m_oldHasOverrides;
        }
    }

    void Redo(Registry& registry) override
    {
        WriteText(m_afterText);
        if (auto* prefab = registry.GetComponent<PrefabInstanceComponent>(m_root)) {
            prefab->hasOverrides = m_newHasOverrides;
        }
    }

    const char* GetName() const override { return "Apply Prefab"; }

private:
    void WriteText(const std::string& text)
    {
        std::ofstream ofs(m_prefabPath, std::ios::binary | std::ios::trunc);
        ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    EntityID m_root = Entity::NULL_ID;
    std::filesystem::path m_prefabPath;
    std::string m_beforeText;
    std::string m_afterText;
    bool m_oldHasOverrides = false;
    bool m_newHasOverrides = false;
};
