#pragma once
#include <cstdint>

// エディタブラシの設定。TerrainEditorPanel が保持する。
struct TerrainBrush {
    enum class Mode { Raise, Lower, Smooth, Flatten, Paint };

    Mode    mode       = Mode::Raise;
    float   radius     = 10.0f;
    float   strength   = 0.5f;
    float   falloff    = 0.5f;
    int32_t layerIndex = 0;
    float   targetHeight = 0.5f; // Flatten モード用
};
