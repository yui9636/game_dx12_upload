#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include <DirectXMath.h>

#include "Entity/Entity.h"

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

struct BlackboardValue
{
    BlackboardValueType type = BlackboardValueType::None;
    int                 i      = 0;
    float               f      = 0.0f;
    DirectX::XMFLOAT3   v3   { 0.0f, 0.0f, 0.0f };
    EntityID            entity = Entity::NULL_ID;
    std::string         s;
};

// Per-entity knowledge base. Owned and read/written ONLY by
// BehaviorTreeSystem and PerceptionSystem.
struct BlackboardComponent
{
    std::unordered_map<std::string, BlackboardValue> entries;
};
