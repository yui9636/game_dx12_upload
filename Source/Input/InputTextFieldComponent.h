#pragma once
#include <cstdint>

struct InputTextFieldComponent {
    bool isFocused = false;
    bool allowMultiline = false;
    uint32_t maxLength = 256;
    bool compositionEnabled = true;
    char compositionText[64] = {};
    int32_t compositionCursor = 0;
    int32_t compositionSelectionLen = 0;
};
