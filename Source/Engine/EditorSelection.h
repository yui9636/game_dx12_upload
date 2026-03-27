#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "Entity/Entity.h" // EntityID???????????????????????

enum class SelectionType {
    None,
    Entity,
    Asset
};

class EditorSelection {
public:
    static EditorSelection& Instance() {
        static EditorSelection instance;
        return instance;
    }

    void Clear() {
        m_type = SelectionType::None;
        m_primaryEntity = 0;
        m_selectedEntities.clear();
        m_selectedAssetPath.clear();
    }

    void SelectEntity(EntityID entity) {
        m_type = SelectionType::Entity;
        m_primaryEntity = entity;
        m_selectedEntities.clear();
        if (!Entity::IsNull(entity)) {
            m_selectedEntities.push_back(entity);
        }
        m_selectedAssetPath.clear();
    }

    void SetEntitySelection(const std::vector<EntityID>& entities, EntityID primaryEntity) {
        m_type = SelectionType::Entity;
        m_selectedAssetPath.clear();
        m_selectedEntities.clear();
        for (EntityID entity : entities) {
            if (Entity::IsNull(entity)) {
                continue;
            }
            if (std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) == m_selectedEntities.end()) {
                m_selectedEntities.push_back(entity);
            }
        }
        if (m_selectedEntities.empty()) {
            Clear();
            return;
        }
        if (std::find(m_selectedEntities.begin(), m_selectedEntities.end(), primaryEntity) == m_selectedEntities.end()) {
            primaryEntity = m_selectedEntities.back();
        }
        m_primaryEntity = primaryEntity;
    }

    void AddEntity(EntityID entity, bool makePrimary = true) {
        if (Entity::IsNull(entity)) {
            return;
        }
        if (m_type != SelectionType::Entity) {
            m_selectedAssetPath.clear();
            m_selectedEntities.clear();
            m_type = SelectionType::Entity;
        }
        if (std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) == m_selectedEntities.end()) {
            m_selectedEntities.push_back(entity);
        }
        if (makePrimary) {
            m_primaryEntity = entity;
        }
    }

    void RemoveEntity(EntityID entity) {
        const auto it = std::remove(m_selectedEntities.begin(), m_selectedEntities.end(), entity);
        if (it == m_selectedEntities.end()) {
            return;
        }
        m_selectedEntities.erase(it, m_selectedEntities.end());
        if (m_selectedEntities.empty()) {
            Clear();
            return;
        }
        if (m_primaryEntity == entity) {
            m_primaryEntity = m_selectedEntities.back();
        }
    }

    void ToggleEntity(EntityID entity, bool makePrimary = true) {
        if (IsEntitySelected(entity)) {
            RemoveEntity(entity);
        } else {
            AddEntity(entity, makePrimary);
        }
    }

    void SelectAsset(const std::string& path) {
        m_type = SelectionType::Asset;
        m_selectedAssetPath = path;
        m_primaryEntity = 0;
        m_selectedEntities.clear();
    }

    SelectionType GetType() const { return m_type; }
    EntityID GetEntity() const { return m_primaryEntity; }
    EntityID GetPrimaryEntity() const { return m_primaryEntity; }
    const std::string& GetAssetPath() const { return m_selectedAssetPath; }
    const std::vector<EntityID>& GetSelectedEntities() const { return m_selectedEntities; }
    size_t GetSelectedEntityCount() const { return m_selectedEntities.size(); }
    bool IsEntitySelected(EntityID entity) const {
        return std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) != m_selectedEntities.end();
    }

private:
    EditorSelection() = default;

    SelectionType m_type = SelectionType::None;
    EntityID m_primaryEntity = 0;
    std::vector<EntityID> m_selectedEntities;
    std::string m_selectedAssetPath;
};
