#pragma once
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include <vector>
#include <tuple>
#include <type_traits>

// Ts... は要求するコンポーネントの型のリスト（例：Query<Transform, Velocity>）
template<typename... Ts>
class Query {
public:
    // コンストラクタで、条件に合致するアーキタイプ（テーブル）を事前に抽出してキャッシュする
    Query(Registry& registry) : m_registry(registry) {
        // 要求された型から、検索用のシグネチャを生成
        m_querySignature = CreateSignature<Ts...>();

        // Registryから全テーブルを取得し、シグネチャが一致するものだけを保持
        // （※実運用では、Registry側にキャッシュさせておくことで更に高速化できます）
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            if (SignatureMatches(archetype->GetSignature(), m_querySignature)) {
                m_matchedArchetypes.push_back(archetype);
            }
        }
    }

    // キャッシュされたテーブル群を回し、ユーザーが渡した関数（ラムダ式）を一気に実行する
    template<typename Func>
    void ForEach(Func&& func) {
        // 1. 合致した各アーキタイプ（テーブル）ごとに処理
        for (Archetype* archetype : m_matchedArchetypes) {
            const size_t entityCount = archetype->GetEntityCount();
            if (entityCount == 0) continue;

            // 2. このテーブルから、要求された全コンポーネントの「列の先頭ポインタ」を取得し、タプルにまとめる
            // （C++17のパック展開を利用しています）
            auto columnPointers = std::make_tuple(
                static_cast<Ts*>(
                    archetype->GetColumn(TypeManager::GetComponentTypeID<Ts>())->Get(0)
                    )...
            );

            // 3. 超高速なインナーループ（ここがエンジンで一番速い場所）
            for (size_t i = 0; i < entityCount; ++i) {
                // タプルに入れた各ポインタを配列として[i]でアクセスし、ラムダ式に渡す
                func((std::get<Ts*>(columnPointers)[i])...);
            }
        }
    }

    template<typename Func>
    void ForEachWithEntity(Func&& func) {
        for (Archetype* archetype : m_matchedArchetypes) {
            const size_t entityCount = archetype->GetEntityCount();
            if (entityCount == 0) continue;

            const auto& entities = archetype->GetEntities(); // 行番号からIDを引くリスト

            auto columnPointers = std::make_tuple(
                static_cast<Ts*>(archetype->GetColumn(TypeManager::GetComponentTypeID<Ts>())->Get(0))...
            );

            for (size_t i = 0; i < entityCount; ++i) {
                // 第一引数にEntityIDを渡し、残りを展開して渡す
                func(entities[i], (std::get<Ts*>(columnPointers)[i])...);
            }
        }
    }

private:
    Registry& m_registry;
    Signature m_querySignature;
    std::vector<Archetype*> m_matchedArchetypes; // 合致したテーブルのリスト
};