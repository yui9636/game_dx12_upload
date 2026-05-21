#pragma once
#include <cstdint>

enum class CoinHUDTag : uint8_t {
    CoinCount  = 0,
    Timer      = 1,
    ResultText = 2,
};

struct CoinHUDTagComponent {
    CoinHUDTag tag = CoinHUDTag::CoinCount;
};
