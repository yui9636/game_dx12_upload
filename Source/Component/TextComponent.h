#pragma once

#include <DirectXMath.h>
#include <string>

enum class TextAlignment
{
    Left,
    Center,
    Right
};

struct TextComponent
{
    std::string text = "Text";
    std::string fontAssetPath = "Data/Font/ArialUni.ttf";
    float fontSize = 32.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextAlignment alignment = TextAlignment::Center;
    float lineSpacing = 1.0f;
    bool wrapping = false;
};
