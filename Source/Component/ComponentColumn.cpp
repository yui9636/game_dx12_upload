#include "ComponentColumn.h"
#include <cstring>
#include <cstdlib>

// ComponentColumn を生成する。
// 1要素サイズと、生成・ムーブ生成・ムーブ代入・破棄の関数ポインタを保存する。
ComponentColumn::ComponentColumn(size_t elementSize, ConstructFn c, MoveConstructFn mc, MoveAssignFn ma, DestructFn d)
    : m_elementSize(elementSize), m_count(0), m_capacity(0), m_data(nullptr),
    m_constructFn(c), m_moveConstructFn(mc), m_moveAssignFn(ma), m_destructFn(d) {
}

// ComponentColumn を破棄する。
// 保持中の全要素に対して destructor を呼び、その後メモリを解放する。
ComponentColumn::~ComponentColumn() {
    if (m_data) {
        // 生存中の全要素を順に破棄する。
        for (size_t i = 0; i < m_count; ++i) {
            m_destructFn(static_cast<char*>(m_data) + i * m_elementSize);
        }

        // 生メモリ領域を解放する。
        std::free(m_data);
    }
}

// ムーブコンストラクタ。
// 他オブジェクトの所有しているバッファをそのまま奪う。
ComponentColumn::ComponentColumn(ComponentColumn&& other) noexcept
    : m_data(other.m_data),
    m_elementSize(other.m_elementSize),
    m_count(other.m_count),
    m_capacity(other.m_capacity),
    m_constructFn(other.m_constructFn),
    m_moveConstructFn(other.m_moveConstructFn),
    m_moveAssignFn(other.m_moveAssignFn),
    m_destructFn(other.m_destructFn)
{
    // 奪ったあとの元オブジェクトは空にしておく。
    other.m_data = nullptr;
    other.m_count = 0;
    other.m_capacity = 0;
}

// ムーブ代入演算子。
// 既存の自前データを破棄してから、相手の所有権を奪う。
ComponentColumn& ComponentColumn::operator=(ComponentColumn&& other) noexcept {
    if (this != &other) {
        // まず自分が今持っているデータを安全に破棄する。
        if (m_data) {
            for (size_t i = 0; i < m_count; ++i) {
                m_destructFn(static_cast<char*>(m_data) + i * m_elementSize);
            }
            std::free(m_data);
        }

        // 相手のデータ一式を受け取る。
        m_data = other.m_data;
        m_elementSize = other.m_elementSize;
        m_count = other.m_count;
        m_capacity = other.m_capacity;
        m_constructFn = other.m_constructFn;
        m_moveConstructFn = other.m_moveConstructFn;
        m_moveAssignFn = other.m_moveAssignFn;
        m_destructFn = other.m_destructFn;

        // 相手は空状態へ戻す。
        other.m_data = nullptr;
        other.m_count = 0;
        other.m_capacity = 0;
    }
    return *this;
}

// 指定容量以上を確保する。
// 既存データがある場合は、新領域へムーブ構築してから旧領域を破棄する。
void ComponentColumn::Reserve(size_t newCapacity) {
    // 既に十分な容量があるなら何もしない。
    if (newCapacity <= m_capacity) return;

    // 新しい生メモリ領域を確保する。
    void* newData = std::malloc(newCapacity * m_elementSize);

    // 既存データがあるなら、新領域へ要素を移し替える。
    if (m_data) {
        for (size_t i = 0; i < m_count; ++i) {
            void* src = static_cast<char*>(m_data) + i * m_elementSize;
            void* dst = static_cast<char*>(newData) + i * m_elementSize;

            // 新領域へムーブ構築する。
            m_moveConstructFn(dst, src);

            // 旧領域の要素を破棄する。
            m_destructFn(src);
        }

        // 旧メモリを解放する。
        std::free(m_data);
    }

    // 新領域へ差し替える。
    m_data = newData;
    m_capacity = newCapacity;
}

// コピー元データを使って 1 要素追加する。
// 必要なら自動で容量拡張する。
void ComponentColumn::Add(const void* pData) {
    // 容量不足なら拡張する。
    if (m_count >= m_capacity) Reserve(m_capacity == 0 ? 8 : m_capacity * 2);

    // 追加先アドレスを計算する。
    void* dst = static_cast<char*>(m_data) + m_count * m_elementSize;

    // コピー元から通常構築する。
    m_constructFn(dst, pData);

    // 要素数を増やす。
    m_count++;
}

// ムーブ元データを使って 1 要素追加する。
// 必要なら自動で容量拡張する。
void ComponentColumn::MoveAdd(void* pData) {
    // 容量不足なら拡張する。
    if (m_count >= m_capacity) Reserve(m_capacity == 0 ? 8 : m_capacity * 2);

    // 追加先アドレスを計算する。
    void* dst = static_cast<char*>(m_data) + m_count * m_elementSize;

    // ムーブ構築で追加する。
    m_moveConstructFn(dst, pData);

    // 要素数を増やす。
    m_count++;
}

// 指定 index の要素を削除する。
// 末尾要素を詰める swap-back 方式で O(1) 削除する。
void ComponentColumn::Remove(size_t index) {
    // 範囲外なら何もしない。
    if (index >= m_count) return;

    void* dst = static_cast<char*>(m_data) + (index * m_elementSize);
    size_t lastIndex = m_count - 1;

    if (index != lastIndex) {
        // 末尾要素を削除位置へムーブ代入する。
        void* src = static_cast<char*>(m_data) + (lastIndex * m_elementSize);
        m_moveAssignFn(dst, src);

        // 元の末尾要素を破棄する。
        m_destructFn(src);
    }
    else {
        // 末尾そのものを削除するだけなら、その場で破棄する。
        m_destructFn(dst);
    }

    // 要素数を減らす。
    m_count--;
}

// 指定 index の要素先頭アドレスを返す。
// 範囲外なら nullptr を返す。
void* ComponentColumn::Get(size_t index) const {
    if (index >= m_count) return nullptr;
    return static_cast<char*>(m_data) + (index * m_elementSize);
}