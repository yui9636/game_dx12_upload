#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

struct EffectCurveKey
{
    float time;
    float value;
    float inTangent;
    float outTangent;
};

struct EffectCurve
{
    std::vector<EffectCurveKey> keys;

    bool IsValid() const { return !keys.empty(); }

    float Evaluate(float t, float defaultValue = 0.0f) const
    {
        if (keys.empty()) return defaultValue;
        if (keys.size() == 1) return keys[0].value;

        if (t <= keys.front().time) return keys.front().value;
        if (t >= keys.back().time) return keys.back().value;

        auto it = std::upper_bound(keys.begin(), keys.end(), t, [](float val, const EffectCurveKey& k) {
            return val < k.time;
            });

        const EffectCurveKey& k1 = *it;
        const EffectCurveKey& k0 = *(it - 1);

        float dt = k1.time - k0.time;
        if (dt <= 0.00001f) return k0.value;

        float t01 = (t - k0.time) / dt;
        float t2 = t01 * t01;
        float t3 = t2 * t01;

        float m0 = k0.outTangent * dt;
        float m1 = k1.inTangent * dt;

        float h00 = 2 * t3 - 3 * t2 + 1;
        float h10 = t3 - 2 * t2 + t01;
        float h01 = -2 * t3 + 3 * t2;
        float h11 = t3 - t2;

        return h00 * k0.value + h10 * m0 + h01 * k1.value + h11 * m1;
    }

    void AddKey(float time, float value, float inTan = 0.0f, float outTan = 0.0f)
    {
        EffectCurveKey k = { time, value, inTan, outTan };
        auto it = std::upper_bound(keys.begin(), keys.end(), time, [](float val, const EffectCurveKey& key) {
            return val < key.time;
            });
        keys.insert(it, k);
    }
};
