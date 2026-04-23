#pragma once
#include <cstddef>
#include <cstdint>

// 1種類の component を連続メモリで保持するための列クラス。
// Archetype 内で、型ごとのデータ列として使う。
class ComponentColumn {
public:
    // 既存データから通常構築する関数。
    using ConstructFn = void(*)(void*, const void*);

    // 既存データからムーブ構築する関数。
    using MoveConstructFn = void(*)(void*, void*);

    // 既存データをムーブ代入する関数。
    using MoveAssignFn = void(*)(void*, void*);

    // 要素を破棄する関数。
    using DestructFn = void(*)(void*);

    // 1要素サイズと、生成・ムーブ・破棄処理を受け取って列を作る。
    ComponentColumn(size_t elementSize, ConstructFn c, MoveConstructFn mc, MoveAssignFn ma, DestructFn d);

    // 保持中の要素を破棄してメモリを解放する。
    ~ComponentColumn();

    // コピーは禁止。
    ComponentColumn(const ComponentColumn&) = delete;
    ComponentColumn& operator=(const ComponentColumn&) = delete;

    // ムーブコンストラクタ。
    ComponentColumn(ComponentColumn&& other) noexcept;

    // ムーブ代入演算子。
    ComponentColumn& operator=(ComponentColumn&& other) noexcept;

    // コピー元データを使って 1 要素追加する。
    void Add(const void* pData);

    // ムーブ元データを使って 1 要素追加する。
    void MoveAdd(void* pData);

    // 指定 index の要素を削除する。
    // 末尾要素を詰める swap-back 方式を想定している。
    void Remove(size_t index);

    // 指定 index の要素先頭アドレスを返す。
    // 範囲外なら nullptr を返す。
    void* Get(size_t index) const;

    // 現在の要素数を返す。
    size_t GetSize() const { return m_count; }

    // 指定容量以上を確保する。
    void Reserve(size_t newCapacity);

private:
    // 生データ本体。
    void* m_data = nullptr;

    // 1要素あたりのサイズ。
    size_t m_elementSize = 0;

    // 現在の要素数。
    size_t m_count = 0;

    // 確保済み容量。
    size_t m_capacity = 0;

    // 通常構築関数。
    ConstructFn m_constructFn = nullptr;

    // ムーブ構築関数。
    MoveConstructFn m_moveConstructFn = nullptr;

    // ムーブ代入関数。
    MoveAssignFn m_moveAssignFn = nullptr;

    // 破棄関数。
    DestructFn m_destructFn = nullptr;
};