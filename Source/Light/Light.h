#pragma once

#include <DirectXMath.h>
#include <vector>
#include"Sprite/Sprite.h"

// 点光源1つ分の描画用データです。
// 位置・範囲・色・強さを RenderContext に渡すために使います。
struct PointLight
{
    // 光源のワールド座標です。
    DirectX::XMFLOAT3 position;

    // 光が届く距離です。
    float range;

    // 光の色です。
    DirectX::XMFLOAT3 color;

    // 光の強さです。
    float intensity;
};

// 平行光源1つ分の描画用データです。
// 太陽光のように、全体へ同じ方向から当たるライトを表します。
struct DirectionalLight
{
    // 光が進む方向です。
    DirectX::XMFLOAT3 direction;

    // 光の色です。
    DirectX::XMFLOAT3 color;
};

