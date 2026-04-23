#include "ArchetypeGraph.h"

// 空の archetype を取得する。
// まだ作られていなければ、空 signature から新しく生成する。
Archetype* ArchetypeGraph::GetEmptyArchetype() {
    // 空 archetype が未生成なら作る。
    if (!m_emptyArchetype) {
        Signature emptySig;
        m_emptyArchetype = CreateArchetype(emptySig);
    }

    // 空 archetype を返す。
    return m_emptyArchetype;
}

// 指定 signature に対応する archetype を検索する。
// 見つからなければ nullptr を返す。
Archetype* ArchetypeGraph::GetArchetype(const Signature& sig) {
    // signature をキーに archetype を検索する。
    auto it = m_archetypes.find(sig);

    // 見つかったらその archetype を返す。
    if (it != m_archetypes.end()) {
        return it->second.get();
    }

    // 無ければ nullptr。
    return nullptr;
}

// 指定 signature の archetype を新規作成する。
// 所有権は m_archetypes が持つ。
Archetype* ArchetypeGraph::CreateArchetype(const Signature& sig) {
    // 新しい archetype を生成する。
    auto archetype = std::make_unique<Archetype>(sig);

    // 生ポインタを控えておく。
    Archetype* ptr = archetype.get();

    // マップへ所有権ごと移す。
    m_archetypes[sig] = std::move(archetype);

    // 生成した archetype を返す。
    return ptr;
}

// 現在 archetype に component を1つ追加した先の archetype を取得する。
// まだ存在しなければ作成し、add/remove edge も張る。
Archetype* ArchetypeGraph::GetOrCreateNextArchetype(Archetype* current, ComponentTypeID typeId, size_t elementSize,
    ComponentColumn::ConstructFn c, ComponentColumn::MoveConstructFn mc,
    ComponentColumn::MoveAssignFn ma, ComponentColumn::DestructFn d) {
    // 現在 archetype に対応する edge 情報を取得する。
    auto& edges = m_edges[current];

    // 既にこの component 追加先が分かっていれば、それを返す。
    auto it = edges.addEdges.find(typeId);
    if (it != edges.addEdges.end()) {
        return it->second;
    }

    // 現在 signature へ component を1つ追加した次 signature を作る。
    Signature nextSig = current->GetSignature();
    nextSig.set(typeId);

    // その signature の archetype が既に存在するか探す。
    Archetype* nextArchetype = GetArchetype(nextSig);

    // 無ければ新規作成する。
    if (!nextArchetype) {
        nextArchetype = CreateArchetype(nextSig);

        // 現在 archetype の列定義をコピーする。
        nextArchetype->CopySchemaFrom(current);

        // 新しく追加された component 用の列を追加する。
        nextArchetype->AddColumn(typeId, elementSize, c, mc, ma, d);
    }

    // 今回求めた add edge をキャッシュする。
    edges.addEdges[typeId] = nextArchetype;

    // 逆方向の remove edge も張っておく。
    m_edges[nextArchetype].removeEdges[typeId] = current;

    // 遷移先 archetype を返す。
    return nextArchetype;
}

// 現在 archetype から component を1つ削除した先の archetype を取得する。
// まだ存在しなければ作成し、add/remove edge も張る。
Archetype* ArchetypeGraph::GetOrCreatePreviousArchetype(Archetype* current, ComponentTypeID typeId) {
    // 現在 archetype に対応する edge 情報を取得する。
    auto& edges = m_edges[current];

    // 既にこの component 削除先が分かっていれば、それを返す。
    auto it = edges.removeEdges.find(typeId);
    if (it != edges.removeEdges.end()) {
        return it->second;
    }

    // 現在 signature から component を1つ外した前 signature を作る。
    Signature nextSig = current->GetSignature();
    nextSig.reset(typeId);

    // その signature の archetype が既に存在するか探す。
    Archetype* prevArchetype = GetArchetype(nextSig);

    // 無ければ新規作成する。
    if (!prevArchetype) {
        prevArchetype = CreateArchetype(nextSig);

        // 現在 archetype の列定義をコピーし、削除対象 component だけ除外する。
        prevArchetype->CopySchemaFromExcluding(current, typeId);
    }

    // 今回求めた remove edge をキャッシュする。
    edges.removeEdges[typeId] = prevArchetype;

    // 逆方向の add edge も張っておく。
    m_edges[prevArchetype].addEdges[typeId] = current;

    // 遷移先 archetype を返す。
    return prevArchetype;
}

// 現在保持しているすべての archetype を配列で返す。
std::vector<Archetype*> ArchetypeGraph::GetAllArchetypes() const {
    // 戻り値用の配列を用意する。
    std::vector<Archetype*> result;

    // 事前に必要数ぶん確保して再確保を減らす。
    result.reserve(m_archetypes.size());

    // すべての archetype を生ポインタで詰める。
    for (const auto& pair : m_archetypes) {
        result.push_back(pair.second.get());
    }

    // 一覧を返す。
    return result;
}