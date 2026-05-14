#pragma once
#include <DirectXMath.h>

/**
 * @brief 描画用の簡易デバッグ形状コンポーネント。
 */
struct GizmoComponent {
    enum class Shape { Box, Sphere, Cylinder, Capsule };

    Shape shape = Shape::Box;
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };

    DirectX::XMFLOAT3 offset = { 0, 0, 0 };
    DirectX::XMFLOAT3 size = { 1, 1, 1 };
    float radius = 0.5f;
    float height = 1.0f;

};
