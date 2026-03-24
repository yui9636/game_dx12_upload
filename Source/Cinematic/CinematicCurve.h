#pragma once
#include "CinematicTypes.h"

namespace Cinematic
{
    template<typename T>
    class Curve
    {
    public:
        std::vector<Keyframe<T>> keys;

        void AddKey(float time, const T& value, InterpolationMode mode = InterpolationMode::CatmullRom)
        {
            auto it = std::find_if(keys.begin(), keys.end(), [time](const Keyframe<T>& k) { return fabsf(k.time - time) < 0.001f; });
            if (it != keys.end())
            {
                it->value = value;
                it->mode = mode;
            }
            else
            {
                keys.push_back({ time, value, mode });
                SortKeys();
            }
        }

        void SortKeys()
        {
            std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
        }

        T Evaluate(float time) const
        {
            if (keys.empty()) return T{};
            if (keys.size() == 1) return keys[0].value;

            if (time <= keys.front().time) return keys.front().value;
            if (time >= keys.back().time) return keys.back().value;

            size_t i = 0;
            for (; i < keys.size() - 1; ++i) {
                if (time < keys[i + 1].time) break;
            }

            const auto& k1 = keys[i];
            const auto& k2 = keys[i + 1];

            float dt = k2.time - k1.time;
            float t = (dt <= 0.00001f) ? 0.0f : (time - k1.time) / dt;

            if (k1.mode == InterpolationMode::Step) return k1.value;
            if (k1.mode == InterpolationMode::Linear) return MathLerp(k1.value, k2.value, t);

            if (k1.mode == InterpolationMode::CatmullRom)
            {
                const auto& k0 = (i > 0) ? keys[i - 1] : k1;
                const auto& k3 = (i + 2 < keys.size()) ? keys[i + 2] : k2;
                return MathCatmullRom(k0.value, k1.value, k2.value, k3.value, t);
            }

            return k1.value;
        }
    };
}
