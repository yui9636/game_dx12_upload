#pragma once
#include <cstdint>

struct InputUserComponent {
    uint8_t userId = 0;
    uint32_t deviceMask = 0xFFFFFFFF; // 既定では全 device を対象にする。
    char profileName[64] = {};
    bool isEditorUser = false;
    bool isPrimary = true;
};
