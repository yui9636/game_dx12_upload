#pragma once
#include <string>
#include "Entity/Entity.h" // EntityIDの定義があるファイルをインクルードしてください

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
        m_selectedEntity = 0;
        m_selectedAssetPath.clear();
    }

    void SelectEntity(EntityID entity) {
        m_type = SelectionType::Entity;
        m_selectedEntity = entity;
        m_selectedAssetPath.clear();
    }

    void SelectAsset(const std::string& path) {
        m_type = SelectionType::Asset;
        m_selectedAssetPath = path;
        m_selectedEntity = 0;
    }

    SelectionType GetType() const { return m_type; }
    EntityID GetEntity() const { return m_selectedEntity; }
    const std::string& GetAssetPath() const { return m_selectedAssetPath; }

private:
    EditorSelection() = default;

    SelectionType m_type = SelectionType::None;
    EntityID m_selectedEntity = 0;
    std::string m_selectedAssetPath;
};