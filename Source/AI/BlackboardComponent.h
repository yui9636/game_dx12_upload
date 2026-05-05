#pragma once

// AI の判断に使うキー値ストアであるブラックボードのコンポーネント定義。
#include <cstdint>
#include <string>
#include <unordered_map>

#include <DirectXMath.h>

#include "Entity/Entity.h"

// BlackboardValue に格納できる値の型。
enum class BlackboardValueType : uint8_t
{
    None    = 0,
    Bool    = 1,
    Int     = 2,
    Float   = 3,
    Vector3 = 4,
    Entity  = 5,
    String  = 6,
};

// ブラックボードに格納する 1 つの値。
struct BlackboardValue
{
    // この値の実際の型。
    BlackboardValueType type = BlackboardValueType::None;
    // bool / int 用の値。
    int                 i      = 0;
    // float 用の値。
    float               f      = 0.0f;
    // Vector3 用の値。
    DirectX::XMFLOAT3   v3   { 0.0f, 0.0f, 0.0f };
    // Entity 参照用の値。
    EntityID            entity = Entity::NULL_ID;
    // string 用の値。
    std::string         s;
};

// AI が共有して使うキー値ストアを保持するコンポーネント。
struct BlackboardComponent
{
    // キー名から BlackboardValue へ引くテーブル。
    std::unordered_map<std::string, BlackboardValue> entries;
};
