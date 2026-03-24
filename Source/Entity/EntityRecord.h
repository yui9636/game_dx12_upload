#pragma once
#include <cstddef>

class Archetype;

struct EntityRecord {
    Archetype* archetype = nullptr;
    size_t row = 0;
};
