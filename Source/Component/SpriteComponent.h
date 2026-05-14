// 画像アセットと色を持つ 2D スプライト。CanvasItem が無い場合は Transform のワールド座標へ描画する。
#pragma once

#include <DirectXMath.h>
#include <string>

struct SpriteComponent
{
    std::string textureAssetPath;
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
};
