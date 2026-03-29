#pragma once

#include <DirectXMath.h>
#include <string>

struct SpriteComponent
{
    std::string textureAssetPath;
    DirectX::XMFLOAT4 tint = { 1.0f, 1.0f, 1.0f, 1.0f };
};
