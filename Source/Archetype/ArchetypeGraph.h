#pragma once
#include "Archetype.h"
#include "Component/ComponentSignature.h"
#include <unordered_map>
#include <memory>

// std::bitsetをunordered_mapのキーとして使うためのハッシュ定義
namespace std {
    template <>
    struct hash<Signature> {
        size_t operator()(const Signature& sig) const {
            return hash<unsigned long long>()(sig.to_ullong());
        }
    };
}

// 1つのアーキタイプから別のアーキタイプへの「道」
struct ArchetypeEdges {
    // 追加時の遷移先キャッシュ (Key: 追加されたTypeID, Value: 次のArchetype)
    std::unordered_map<ComponentTypeID, Archetype*> addEdges;

    // 削除時の遷移先キャッシュ (Key: 削除されたTypeID, Value: 次のArchetype)
    std::unordered_map<ComponentTypeID, Archetype*> removeEdges;
};

class ArchetypeGraph {
public:
    ArchetypeGraph() = default;
    ~ArchetypeGraph() = default;

    // 初期状態（コンポーネントゼロ）の空アーキタイプを取得
    Archetype* GetEmptyArchetype();

    Archetype* GetOrCreateNextArchetype(Archetype* current, ComponentTypeID typeId, size_t elementSize,
        ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
        ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d);

    // コンポーネントを削除した際の「引っ越し先」を取得（なければ自動生成）
    Archetype* GetOrCreatePreviousArchetype(Archetype* current, ComponentTypeID typeId);

    std::vector<Archetype*> GetAllArchetypes() const;
private:
    // シグネチャからArchetypeを検索
    Archetype* GetArchetype(const Signature& sig);

    // 新しいArchetypeをメモリ上に生成
    Archetype* CreateArchetype(const Signature& sig);

private:
    // メモリの実体管理（unique_ptrで安全に破棄）
    std::unordered_map<Signature, std::unique_ptr<Archetype>> m_archetypes;

    // 各アーキタイプの遷移エッジ（地図）
    std::unordered_map<Archetype*, ArchetypeEdges> m_edges;

    // 何も持っていない空のアーキタイプ（全てのエンティティはここから生まれる）
    Archetype* m_emptyArchetype = nullptr;
};