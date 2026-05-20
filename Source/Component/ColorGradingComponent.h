// ColorGradingComponent: applies a color-grading LUT to the final image.
// The LUT is a horizontal-strip 2D texture: N slices of N x N laid left-to-right
// (texture size = (N*N) x N). N is auto-detected from the texture height.
#pragma once
#include <string>

struct ColorGradingComponent {
    std::string lutTexturePath;   // horizontal-strip LUT texture (e.g. 256x16, 1024x32)
    float intensity = 1.0f;       // blend strength [0,1]
};
