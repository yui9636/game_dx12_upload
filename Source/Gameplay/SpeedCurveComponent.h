#pragma once
#include <cstdint>

struct CurvePoint {
    float t01 = 0.0f;
    float value = 1.0f;
};

struct SpeedCurveComponent {
    static constexpr int MAX_POINTS = 16;
    bool enabled = false;
    bool useRangeSpace = false;
    CurvePoint points[MAX_POINTS] = {};
    uint8_t pointCount = 0;
};
