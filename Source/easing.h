#pragma once
#include <cmath>

#ifndef PI
#define PI 3.1415926545f
#endif

template<class T>
constexpr const T& Clamp(const T& value,
    const T& low,
    const T& high)
{
    return (value < low) ? low : (high < value) ? high : value;
}



namespace Easing {

    // easeInExpo: 指数関数的に加速
    inline float easeInExpo(float t) {
        return (t == 0) ? 0 : powf(2, 10 * (t - 1));
    }

    // easeOutExpo: 指数関数的に減速
    inline float easeOutExpo(float t) {
        return (t == 1) ? 1 : 1 - powf(2, -10 * t);
    }

    // easeInOutExpo: 指数関数的に加速・減速
    inline float easeInOutExpo(float t) {
        return (t < 0.5f) ? powf(2, 10 * (2 * t - 1)) / 2 : (2 - powf(2, -10 * (2 * t - 1))) / 2;
    }

    // easeInQuad: 二次関数的に加速
    inline float easeInQuad(float t) {
        return t * t;
    }

    // easeOutQuad: 二次関数的に減速
    inline float easeOutQuad(float t) {
        return t * (2 - t);
    }

    // easeInOutQuad: 二次関数的に加速・減速
    inline float easeInOutQuad(float t) {
        return (t < 0.5f) ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }

    // easeInCubic: 三次関数的に加速
    inline float easeInCubic(float t) {
        return t * t * t;
    }

    // easeOutCubic: 三次関数的に減速
    inline float easeOutCubic(float t) {
        return --t * t * t + 1;
    }

    // easeInOutCubic: 三次関数的に加速・減速
    inline float easeInOutCubic(float t) {
        return (t < 0.5f) ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
    }

    // easeInQuart: 四次関数的に加速
    inline float easeInQuart(float t) {
        return t * t * t * t;
    }

    // easeOutQuart: 四次関数的に減速
    inline float easeOutQuart(float t) {
        return 1 - --t * t * t * t;
    }

    // easeInOutQuart: 四次関数的に加速・減速
    inline float easeInOutQuart(float t) {
        return (t < 0.5f) ? 8 * t * t * t * t : 1 - 8 * --t * t * t * t;
    }

    // easeInSine: 正弦関数的に加速
    inline float easeInSine(float t) {
        return 1 - cosf(t * (PI / 2));
    }

    // easeOutSine: 正弦関数的に減速
    inline float easeOutSine(float t) {
        return sinf(t * (PI / 2));
    }

    // easeInOutSine: 正弦関数的に加速・減速
    inline float easeInOutSine(float t) {
        return 0.5f * (1 - cosf(t * PI));
    }

    // easeInCirc: 円関数的に加速
    inline float easeInCirc(float t) {
        return 1 - sqrtf(1 - t * t);
    }

    // easeOutCirc: 円関数的に減速
    inline float easeOutCirc(float t) {
        return sqrtf(1 - --t * t);
    }

    // easeInOutCirc: 円関数的に加速・減速
    inline float easeInOutCirc(float t) {
        return (t < 0.5f) ? (1 - sqrtf(1 - 4 * t * t)) * 0.5f : (sqrtf(1 - (t * 2 - 2) * (t * 2 - 2)) + 1) * 0.5f;
    }

    // easeInBack: バック関数的に加速
    inline float easeInBack(float t) {
        const float s = 1.70158f; // ストレッチファクター
        return t * t * ((s + 1) * t - s);
    }

    // easeOutBack: バック関数的に減速
    inline float easeOutBack(float t) {
        const float s = 1.70158f; // ストレッチファクター
        return --t * t * ((s + 1) * t + s) + 1;
    }

    // easeInOutBack: バック関数的に加速・減速
    inline float easeInOutBack(float t) {
        const float s = 1.70158f * 1.525f;
        return (t < 0.5f) ? (t * t * ((s + 1) * 2 * t - s)) * 0.5f : (t - 1) * (t - 1) * ((s + 1) * (t * 2 - 2) + s) * 0.5f + 1;
    }

    // easeInElastic: バウンドするように加速
    inline float easeInElastic(float t) {
        const float c4 = (2 * PI) / 3;
        return (t == 0) ? 0 : (t == 1) ? 1 : -powf(2, 10 * (t - 1)) * sinf((t * 10 - 0.75f) * c4);
    }

    // easeOutElastic: バウンドするように減速
    inline float easeOutElastic(float t) {
        const float c4 = (2 * PI) / 3;
        return (t == 0) ? 0 : (t == 1) ? 1 : powf(2, -10 * t) * sinf((t * 10 - 0.75f) * c4) + 1;
    }

    // easeInOutElastic: バウンドするように加速・減速
    inline float easeInOutElastic(float t) {
        const float c5 = (2 * PI) / 4.5f;
        return (t == 0) ? 0 : (t == 1) ? 1 : (t < 0.5f) ? -(powf(2, 20 * t - 10) * sinf((20 * t - 11.125f) * c5)) * 0.5f : (powf(2, -20 * t + 10) * sinf((20 * t - 11.125f) * c5)) * 0.5f + 1;
    }

    // easeSway: 左右に揺れる
    inline float easeSway(float t, float amplitude, float frequency) {
        // サイン波を使用して左右に揺れる
        return amplitude * sinf(frequency * t * 2 * PI);
    }

    // easeOutBounce: バウンスしながら減速
    inline float easeOutBounce(float t) {
        if (t < (1 / 2.75f)) {
            return 7.5625f * t * t;
        }
        else if (t < (2 / 2.75f)) {
            t -= (1.5f / 2.75f);
            return 7.5625f * t * t + 0.75f;
        }
        else if (t < (2.5 / 2.75)) {
            t -= (2.25f / 2.75f);
            return 7.5625f * t * t + 0.9375f;
        }
        else {
            t -= (2.625f / 2.75f);
            return 7.5625f * t * t + 0.984375f;
        }
    }

    // easeInBounce: バウンスしながら加速
    inline float easeInBounce(float t) {
        return 1 - easeOutBounce(1 - t);
    }

    // easeInOutBounce: バウンスしながら加速・減速
    inline float easeInOutBounce(float t) {
        return (t < 0.5f) ? (easeInBounce(t * 2) * 0.5f) : (easeOutBounce(t * 2 - 1) * 0.5f + 0.5f);
    }

    // easeFadeOut: フェードアウト効果
    inline float easeFadeOut(float t) {
        return 1 - t * t; // tが1に近づくと0に近づく
    }
}
