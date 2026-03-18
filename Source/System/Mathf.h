#pragma once

// 浮動小数算術
class Mathf
{
public:
    // 線形補完
    static float Lerp(float a, float b, float t);

    // 指定範囲のランダム値を計算する
    static float RandomRange(float min, float max);
};