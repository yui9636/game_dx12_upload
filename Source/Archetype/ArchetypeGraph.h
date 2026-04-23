#pragma once

#include "Archetype.h"
#include "Component/ComponentSignature.h"
#include <unordered_map>
#include <memory>

// Signature を unordered_map のキーとして使うための hash 定義。
// component の有無ビット列から安定した hash 値を作る。
namespace std {
    template <>
    struct hash<Signature> {
        size_t operator()(const Signature& sig) const {
            // 初期 seed を設定する。
            size_t seed = 1469598103934665603ull;

            // 全 component bit を見て hash を混ぜる。
            for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
                // bit が立っているかどうかで異なる定数を使う。
                const size_t bitValue = sig[i] ? 0x9e3779b97f4a7c15ull : 0x4f1bbcdc6762c3d1ull;

                // hash を更新する。
                seed ^= bitValue + (seed << 6) + (seed >> 2);
            }

            return seed;
        }
    };
}

// Archetype 間の遷移先を保持する構造体。
// component を追加した時の遷移先と、削除した時の遷移先を分けて持つ。
struct ArchetypeEdges {
    // 指定 component を追加した時に遷移する archetype。
    std::unordered_map<ComponentTypeID, Archetype*> addEdges;

    // 指定 component を削除した時に遷移する archetype。
    std::unordered_map<ComponentTypeID, Archetype*> removeEdges;
};

// Archetype を signature ごとに管理し、
// component の追加・削除に応じた次 archetype への遷移をキャッシュするクラス。
class ArchetypeGraph {
public:
    // デフォルト構築。
    ArchetypeGraph() = default;

    // 特別な解放処理は不要なので default destructor を使う。
    ~ArchetypeGraph() = default;

    // component を1つも持たない空 archetype を取得する。
    Archetype* GetEmptyArchetype();

    // 現在の archetype に component を1つ追加した先の archetype を取得する。
    // まだ存在しなければ新しく作成する。
    Archetype* GetOrCreateNextArchetype(Archetype* current, ComponentTypeID typeId, size_t elementSize,
        ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
        ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d);

    // 現在の archetype から component を1つ削除した先の archetype を取得する。
    // まだ存在しなければ新しく作成する。
    Archetype* GetOrCreatePreviousArchetype(Archetype* current, ComponentTypeID typeId);

    // 現在 graph が保持している全 archetype を配列で返す。
    std::vector<Archetype*> GetAllArchetypes() const;

private:
    // 指定 signature に対応する archetype を取得する。
    // 無ければ nullptr を返す。
    Archetype* GetArchetype(const Signature& sig);

    // 指定 signature の archetype を新規作成する。
    Archetype* CreateArchetype(const Signature& sig);

private:
    // signature ごとに archetype を所有するマップ。
    std::unordered_map<Signature, std::unique_ptr<Archetype>> m_archetypes;

    // 各 archetype からの add / remove 遷移先キャッシュ。
    std::unordered_map<Archetype*, ArchetypeEdges> m_edges;

    // 空 signature に対応する archetype。
    Archetype* m_emptyArchetype = nullptr;
};