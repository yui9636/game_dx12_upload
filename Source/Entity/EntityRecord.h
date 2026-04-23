#pragma once
#include <cstddef>

class Archetype;

// Entity が現在どの Archetype の何行目に居るかを表す管理情報。
// Registry 側で EntityID から実データ位置を引くために使う。
struct EntityRecord {
    // この Entity が所属している Archetype。
    Archetype* archetype = nullptr;

    // Archetype 内の行番号。
    size_t row = 0;
};