#include "Mathf.h"
#include <cstdlib>

float Mathf::Lerp(float a, float b, float t)
{
    return a * (1.0f - t) + (b * t);
}

float Mathf::RandomRange(float min, float max)
{
    // 指定範囲のランダム値を計算する
    float value = static_cast<float>(rand()) / RAND_MAX;
    return min + value * (max - min); // min から max までの範囲にスケール
}