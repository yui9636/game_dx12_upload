#pragma once
#include <cstdint>

struct InputUserComponent {
    uint8_t userId = 0;
    uint32_t deviceMask = 0xFFFFFFFF; // all devices by default
    char profileName[64] = {};
    bool isEditorUser = false;
    bool isPrimary = true;
};
