#pragma once
#include <cstdint>
// PlayerTagComponent は playerId を保持し、関連システムが実行時状態として参照する。

struct PlayerTagComponent {
    uint8_t playerId = 0;
};
