#pragma once
// シネマティック機能で共有する型・補間関数を定義するヘッダー。
#include <DirectXMath.h>
#include"JSONManager.h"
#include <vector>
#include <algorithm>


using namespace DirectX;

// シーケンサー関連の型をまとめる名前空間。
namespace Cinematic
{
    // キーフレーム間の補間方法。
    enum class InterpolationMode
    {
        // 前のキーの値をそのまま維持する。
        Step,
        // 2つのキーの値を直線補間する。
        Linear,
        // 前後のキーも利用して滑らかに補間する。
        CatmullRom,
        // 将来用のベジェ補間指定。
        Bezier
    };

    // 任意の型 T を時間に紐付けて保存するキーフレーム。
    template<typename T>
    struct Keyframe
    {
        // このキーが置かれているシーケンス上の時間。
        float time;
        // この時間で保持する値。
        T value;
        // 次のキーまでの補間方法。
        InterpolationMode mode = InterpolationMode::CatmullRom;
        // ベジェ補間などで使う入口タンジェント。
        T tangentIn{};
        // ベジェ補間などで使う出口タンジェント。
        T tangentOut{};

    };

    // Keyframe を JSON に保存するための変換関数。
    template<typename T>
    inline void to_json(json& j, const Keyframe<T>& k) {
        j = json{
            {"t", k.time},
            {"v", k.value},
            {"m", (int)k.mode}
        };
    }

    // JSON から Keyframe を復元するための変換関数。
    template<typename T>
    inline void from_json(const json& j, Keyframe<T>& k) {
        j.at("t").get_to(k.time);
        j.at("v").get_to(k.value);
        k.mode = (InterpolationMode)j.at("m").get<int>();
    }


    // float 値。 用の線形補間。
    inline float MathLerp(float a, float b, float t) { return a + (b - a) * t; }

    // float 値。 用の Catmull-Rom 補間。
    inline float MathCatmullRom(float v0, float v1, float v2, float v3, float t)
    {
        // t の二乗・三乗を先に求めて式を読みやすくする。
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * v1) + (-v0 + v2) * t + (2.0f * v0 - 5.0f * v1 + 4.0f * v2 - v3) * t2 + (-v0 + 3.0f * v1 - 3.0f * v2 + v3) * t3);
    }

    // XMFLOAT3 用の線形補間。
    inline DirectX::XMFLOAT3 MathLerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
    {
        // DirectXMath のベクトルへ変換して補間する。
        XMVECTOR V1 = XMLoadFloat3(&a);
        XMVECTOR V2 = XMLoadFloat3(&b);
        XMVECTOR R = XMVectorLerp(V1, V2, t);
        XMFLOAT3 res; XMStoreFloat3(&res, R);
        return res;
    }

    // XMFLOAT3 用の Catmull-Rom 補間。
    inline DirectX::XMFLOAT3 MathCatmullRom(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT3& v3, float t)
    {
        // DirectXMath の Catmull-Rom 関数を使って滑らかに補間する。
        XMVECTOR V0 = XMLoadFloat3(&v0);
        XMVECTOR V1 = XMLoadFloat3(&v1);
        XMVECTOR V2 = XMLoadFloat3(&v2);
        XMVECTOR V3 = XMLoadFloat3(&v3);
        XMVECTOR R = XMVectorCatmullRom(V0, V1, V2, V3, t);
        XMFLOAT3 res; XMStoreFloat3(&res, R);
        return res;
    }
}
