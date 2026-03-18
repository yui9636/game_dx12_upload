#pragma once
#include <cstddef>

class Archetype;

// エンティティの現在の「住所」
struct EntityRecord {
    Archetype* archetype = nullptr; // 所属しているテーブル
    size_t row = 0;                 // テーブル内の行番号（インデックス）
};