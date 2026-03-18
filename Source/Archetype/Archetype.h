#pragma once
#include "Entity/Entity.h"
#include "Component/ComponentSignature.h"
#include "Component/ComponentColumn.h"
#include <vector>
#include <unordered_map>

class Archetype {
public:
    Archetype(Signature sig);
    ~Archetype() = default;

    // このアーキタイプが持つシグネチャ
    Signature GetSignature() const { return m_signature; }

    // テーブルに新しい行（エンティティ）を追加し、その行番号(インデックス)を返す
    size_t AddEntity(EntityID entity);

    // テーブルからエンティティを削除（Swap & Popを実行）
    // 戻り値: 代わりに場所が移動した(最後尾にいた)エンティティのID。
    // ※ Registryが住所録を更新するために必要です
    EntityID RemoveEntity(size_t index);

    // 新しい列（コンポーネント）をアーキタイプに登録する（初期化時のみ使用）
    void AddColumn(ComponentTypeID typeId, size_t elementSize,
        ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
        ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d);

    void CopySchemaFrom(const Archetype* other);

    // ★追加: 特定のコンポーネントを除外して列の構成をコピーする (RemoveComponent用)
    void CopySchemaFromExcluding(const Archetype* other, ComponentTypeID excludeTypeId);

    // 特定のコンポーネントの列データへのポインタを取得する
    ComponentColumn* GetColumn(ComponentTypeID typeId);

    // このテーブルにいるエンティティの総数
    size_t GetEntityCount() const { return m_entityIDs.size(); }

    // テーブル内の全エンティティIDリスト
    const std::vector<EntityID>& GetEntities() const { return m_entityIDs; }

private:
    Signature m_signature;

    // 行番号(インデックス)からEntityIDを引くための配列
    std::vector<EntityID> m_entityIDs;

    // ComponentTypeID をキーにして、m_columnsの何番目かを引くためのマップ
    std::unordered_map<ComponentTypeID, size_t> m_typeToIndex;

    struct ColumnSchema {
        size_t elementSize;
        ComponentColumn::ConstructFn constructFn;
        ComponentColumn::MoveConstructFn moveConstructFn;
        ComponentColumn::MoveAssignFn moveAssignFn;
        ComponentColumn::DestructFn destructFn;
    };
    std::unordered_map<ComponentTypeID, ColumnSchema> m_schemas;


    std::unordered_map<ComponentTypeID, size_t> m_elementSizes;

    // 実際のデータ列の配列
    std::vector<ComponentColumn> m_columns;
};