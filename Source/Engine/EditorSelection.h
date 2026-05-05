#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "Entity/Entity.h" // EntityID を扱うために必要

// エディタ上で現在選択している対象の種類を表す列挙型。
enum class SelectionType {
    // 何も選択していない状態。
    None,

    // ECS の Entity を選択している状態。
    Entity,

    // Asset Browser 上のアセットを選択している状態。
    Asset
};

// エディタ全体で共有する選択状態を管理するクラス。
// Hierarchy、Inspector、SceneView、AssetBrowser などが同じ選択情報を参照する。
class EditorSelection {
public:
    // EditorSelection の singleton インスタンスを取得する。
    static EditorSelection& Instance() {
        static EditorSelection instance;
        return instance;
    }

    // 現在の選択状態をすべて解除する。
    void Clear() {
        m_type = SelectionType::None;
        m_primaryEntity = 0;
        m_selectedEntities.clear();
        m_selectedAssetPath.clear();
    }

    // 指定した Entity を単体選択する。
    // 既存の複数選択やアセット選択は解除される。
    void SelectEntity(EntityID entity) {
        m_type = SelectionType::Entity;
        m_primaryEntity = entity;
        m_selectedEntities.clear();
        if (!Entity::IsNull(entity)) {
            m_selectedEntities.push_back(entity);
        }
        m_selectedAssetPath.clear();
    }

    // 複数 Entity の選択状態をまとめて設定する。
    // primaryEntity が選択リストに含まれない場合は、最後の Entity を primary にする。
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

    // 指定した Entity を選択リストへ追加する。
    // makePrimary が true の場合、その Entity を Inspector 表示などの基準にする。
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

    // 指定した Entity を選択リストから外す。
    // primaryEntity が外された場合は、残っている最後の Entity を primary にする。
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

    // 指定した Entity の選択状態を反転する。
    // 選択済みなら解除し、未選択なら追加する。
    void ToggleEntity(EntityID entity, bool makePrimary = true) {
        if (IsEntitySelected(entity)) {
            RemoveEntity(entity);
        } else {
            AddEntity(entity, makePrimary);
        }
    }

    // Asset Browser 上のアセットを選択する。
    // Entity の選択状態は解除される。
    void SelectAsset(const std::string& path) {
        m_type = SelectionType::Asset;
        m_selectedAssetPath = path;
        m_primaryEntity = 0;
        m_selectedEntities.clear();
    }

    // 現在の選択種別を取得する。
    SelectionType GetType() const { return m_type; }

    // 現在の primary Entity を取得する。
    EntityID GetEntity() const { return m_primaryEntity; }

    // 現在の primary Entity を取得する。
    EntityID GetPrimaryEntity() const { return m_primaryEntity; }

    // 現在選択しているアセットパスを取得する。
    const std::string& GetAssetPath() const { return m_selectedAssetPath; }

    // 現在選択している Entity 一覧を取得する。
    const std::vector<EntityID>& GetSelectedEntities() const { return m_selectedEntities; }

    // 現在選択している Entity 数を取得する。
    size_t GetSelectedEntityCount() const { return m_selectedEntities.size(); }

    // 指定した Entity が現在選択されているかを調べる。
    bool IsEntitySelected(EntityID entity) const {
        return std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) != m_selectedEntities.end();
    }

private:
    // singleton 専用なので外部から生成させない。
    EditorSelection() = default;

    // 現在の選択種別。
    SelectionType m_type = SelectionType::None;

    // Inspector 表示や操作の基準になる Entity。
    EntityID m_primaryEntity = 0;

    // 複数選択中の Entity 一覧。
    std::vector<EntityID> m_selectedEntities;

    // Asset 選択中のパス。
    std::string m_selectedAssetPath;
};
