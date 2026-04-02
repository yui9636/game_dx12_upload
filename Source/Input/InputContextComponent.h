#pragma once
#include <cstdint>

enum class InputContextPriority : uint8_t {
    EditorGlobal = 0,
    SceneView = 1,
    RuntimeGameplay = 2,
    RuntimeUI = 3,
    Console = 4,
    TextInput = 5,
    ModalDialog = 6
};

struct InputContextComponent {
    InputContextPriority priority = InputContextPriority::RuntimeGameplay;
    uint32_t activeMapIndices[4] = {};
    uint8_t activeMapCount = 0;
    bool consumeLowerPriority = false;
    bool textInputEnabled = false;
    bool uiNavigationEnabled = false;
    bool pointerEnabled = true;
    bool enabled = true;
    bool consumed = false; // set by InputContextSystem when suppressed
};
