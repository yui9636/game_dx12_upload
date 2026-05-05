#pragma once
// キーフレーム列を保持し、指定時間の値を評価する汎用カーブ。
#include "CinematicTypes.h"

// シネマティック用の補間カーブをまとめる名前空間。
namespace Cinematic
{
    // T 型の値を時間に沿って補間するカーブ。
    template<typename T>
    class Curve
    {
    public:
        // 時間順に並ぶキーフレーム一覧。
        std::vector<Keyframe<T>> keys;

        // 指定時間にキーを追加する。同じ時間のキーがあれば値と補間方法を上書きする。
        void AddKey(float time, const T& value, InterpolationMode mode = InterpolationMode::CatmullRom)
        {
            // 完全一致ではなく誤差を許容して、既存キーとの重複を判定する。
            auto it = std::find_if(keys.begin(), keys.end(), [time](const Keyframe<T>& k) { return fabsf(k.time - time) < 0.001f; });
            if (it != keys.end())
            {
                // 既存キーが見つかった場合は、そのキーを更新する。
                it->value = value;
                it->mode = mode;
            }
            else
            {
                // 新しいキーの場合は追加してから時間順に並べ直す。
                keys.push_back({ time, value, mode });
                SortKeys();
            }
        }

        // キーを時間の昇順に並べる。
        void SortKeys()
        {
            std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
        }

        // 指定時間の値を現在の補間モードに従って計算する。
        T Evaluate(float time) const
        {
            // キーが無い場合は型の初期値を返す。
            if (keys.empty()) return T{};
            // キーが1つだけなら補間せず、その値を返す。
            if (keys.size() == 1) return keys[0].value;

            // 範囲外の時間は先頭または末尾の値に丸める。
            if (time <= keys.front().time) return keys.front().value;
            if (time >= keys.back().time) return keys.back().value;

            // time を挟む2つのキーを探す。
            size_t i = 0;
            for (; i < keys.size() - 1; ++i) {
                if (time < keys[i + 1].time) break;
            }

            const auto& k1 = keys[i];
            const auto& k2 = keys[i + 1];

            // 2つのキー間で 0.0～1.0 の補間率を作る。
            float dt = k2.time - k1.time;
            float t = (dt <= 0.00001f) ? 0.0f : (time - k1.time) / dt;

            // キーに設定された補間方式で値を返す。
            if (k1.mode == InterpolationMode::Step) return k1.value;
            if (k1.mode == InterpolationMode::Linear) return MathLerp(k1.value, k2.value, t);

            if (k1.mode == InterpolationMode::CatmullRom)
            {
                // Catmull-Rom は前後のキーも使う。端では現在キーを代用する。
                const auto& k0 = (i > 0) ? keys[i - 1] : k1;
                const auto& k3 = (i + 2 < keys.size()) ? keys[i + 2] : k2;
                return MathCatmullRom(k0.value, k1.value, k2.value, k3.value, t);
            }

            return k1.value;
        }
    };
}
