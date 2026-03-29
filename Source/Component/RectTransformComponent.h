#pragma once

#include <DirectXMath.h>

struct RectTransformComponent
{
    DirectX::XMFLOAT2 anchoredPosition = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 sizeDelta = { 100.0f, 100.0f };
    DirectX::XMFLOAT2 anchorMin = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 anchorMax = { 0.5f, 0.5f };
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    float rotationZ = 0.0f;
    DirectX::XMFLOAT2 scale2D = { 1.0f, 1.0f };
};
