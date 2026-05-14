#include "Mathf.h"
#include <cstdlib>
// Mathf::Lerp はこのモジュールの実行時処理を構成する補助処理を行う。

float Mathf::Lerp(float a, float b, float t)
{
    return a * (1.0f - t) + (b * t);
}

float Mathf::RandomRange(float min, float max)
{
    float value = static_cast<float>(rand()) / RAND_MAX;
    return min + value * (max - min);
}
