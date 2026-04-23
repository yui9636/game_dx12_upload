#pragma once

#include "Entity/Entity.h"
#include "Component/ComponentSignature.h"
#include "Component/ComponentColumn.h"
#include <vector>
#include <unordered_map>

// Archetype は、同じ component 構成を持つ entity 群をまとめて保持するクラス。
// ECS の archetype 方式における 1 つのテーブルに相当し、
// entity ID 配列と component 列配列を並行して持つ。
class Archetype {
public:
    // 指定 signature を持つ archetype を生成する。
    explicit Archetype(Signature sig);

    // 特別な解放処理は不要なので default destructor を使う。
    ~Archetype() = default;

    // この archetype が表す component 構成を返す。
    Signature GetSignature() const { return m_signature; }

    // entity を archetype に追加する。
    // 戻り値は追加された行 index。
    size_t AddEntity(EntityID entity);

    // 指定 index の entity を削除する。
    // 戻り値は swap-back 等で移動してきた entity ID。
    // 実装次第では、移動が無い場合に同じ entity または無効値を返すことがある。
    EntityID RemoveEntity(size_t index);

    // 新しい component 列を追加する。
    // typeId に対応する列を作り、1 要素サイズと生成・移動・破棄関数を登録する。
    void AddColumn(ComponentTypeID typeId, size_t elementSize,
        ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
        ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d);

    // 他 archetype の列定義と schema 情報をそのままコピーする。
    // entity の中身まではコピーせず、「どんな列を持つか」だけ複製する用途。
    void CopySchemaFrom(const Archetype* other);

    // 他 archetype の列定義をコピーするが、指定 typeId の列だけ除外する。
    // component 削除遷移時の新 archetype 構築などで使う想定。
    void CopySchemaFromExcluding(const Archetype* other, ComponentTypeID excludeTypeId);

    // 指定 component typeId に対応する列を取得する。
    // 存在しなければ nullptr を返す。
    ComponentColumn* GetColumn(ComponentTypeID typeId);

    // 現在この archetype に所属する entity 数を返す。
    size_t GetEntityCount() const { return m_entityIDs.size(); }

    // archetype に属する entity ID 配列を返す。
    const std::vector<EntityID>& GetEntities() const { return m_entityIDs; }

private:
    // この archetype が表す component 構成。
    Signature m_signature;

    // archetype に属する entity ID の配列。
    // 各列の同じ index 行が、この entity に対応する。
    std::vector<EntityID> m_entityIDs;

    // component typeId から m_columns の index を引くための対応表。
    std::unordered_map<ComponentTypeID, size_t> m_typeToIndex;

    // 各 component 列の生成・移動・破棄方法を保持する schema 情報。
    struct ColumnSchema {
        // 1 要素あたりのサイズ。
        size_t elementSize;

        // デフォルト構築関数。
        ComponentColumn::ConstructFn constructFn;

        // ムーブ構築関数。
        ComponentColumn::MoveConstructFn moveConstructFn;

        // ムーブ代入関数。
        ComponentColumn::MoveAssignFn moveAssignFn;

        // 破棄関数。
        ComponentColumn::DestructFn destructFn;
    };

    // component typeId ごとの schema 一覧。
    std::unordered_map<ComponentTypeID, ColumnSchema> m_schemas;

    // component typeId ごとの要素サイズ。
    // schema と一部重複するが、用途を分けて保持している。
    std::unordered_map<ComponentTypeID, size_t> m_elementSizes;

    // 実際の component 列本体。
    // m_typeToIndex を使って typeId から対応列へアクセスする。
    std::vector<ComponentColumn> m_columns;
};