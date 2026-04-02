#pragma once
#include "InputEvent.h"
#include <cstdint>

struct InputDebugStateComponent {
    static constexpr int HISTORY_SIZE = 64;
    InputEvent history[HISTORY_SIZE] = {};
    uint16_t historyHead = 0;
    uint16_t historyCount = 0;
    bool recordEnabled = true;

    void PushEvent(const InputEvent& ev) {
        if (!recordEnabled) return;
        history[historyHead] = ev;
        historyHead = (historyHead + 1) % HISTORY_SIZE;
        if (historyCount < HISTORY_SIZE) historyCount++;
    }
};
