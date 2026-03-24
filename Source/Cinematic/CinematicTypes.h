#pragma once
#include <DirectXMath.h>
#include"JSONManager.h"
#include <vector>
#include <algorithm>


using namespace DirectX;

namespace Cinematic
{
    enum class InterpolationMode
    {
        Step,
        Linear,
        CatmullRom,
        Bezier
    };

    template<typename T>
    struct Keyframe
    {
        float time;
        T value;
        InterpolationMode mode = InterpolationMode::CatmullRom;
        T tangentIn{};
        T tangentOut{};

    };

    template<typename T>
    inline void to_json(json& j, const Keyframe<T>& k) {
        j = json{
            {"t", k.time},
            {"v", k.value},
            {"m", (int)k.mode}
        };
    }

    template<typename T>
    inline void from_json(const json& j, Keyframe<T>& k) {
        j.at("t").get_to(k.time);
        j.at("v").get_to(k.value);
        k.mode = (InterpolationMode)j.at("m").get<int>();
    }


    inline float MathLerp(float a, float b, float t) { return a + (b - a) * t; }

    inline float MathCatmullRom(float v0, float v1, float v2, float v3, float t)
    {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * v1) + (-v0 + v2) * t + (2.0f * v0 - 5.0f * v1 + 4.0f * v2 - v3) * t2 + (-v0 + 3.0f * v1 - 3.0f * v2 + v3) * t3);
    }

    inline DirectX::XMFLOAT3 MathLerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
    {
        XMVECTOR V1 = XMLoadFloat3(&a);
        XMVECTOR V2 = XMLoadFloat3(&b);
        XMVECTOR R = XMVectorLerp(V1, V2, t);
        XMFLOAT3 res; XMStoreFloat3(&res, R);
        return res;
    }

    inline DirectX::XMFLOAT3 MathCatmullRom(const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT3& v3, float t)
    {
        XMVECTOR V0 = XMLoadFloat3(&v0);
        XMVECTOR V1 = XMLoadFloat3(&v1);
        XMVECTOR V2 = XMLoadFloat3(&v2);
        XMVECTOR V3 = XMLoadFloat3(&v3);
        XMVECTOR R = XMVectorCatmullRom(V0, V1, V2, V3, t);
        XMFLOAT3 res; XMStoreFloat3(&res, R);
        return res;
    }
}
