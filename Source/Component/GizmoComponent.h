#pragma once
#include <DirectXMath.h>

/**
 * @brief 汎用的なデバッグ表示用コンポーネント
 */
struct GizmoComponent {
    enum class Shape { Box, Sphere, Cylinder, Capsule };

    Shape shape = Shape::Box;
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };

    // TransformComponent からの相対オフセットとサイズ
    DirectX::XMFLOAT3 offset = { 0, 0, 0 };
    DirectX::XMFLOAT3 size = { 1, 1, 1 }; // Box用
    float radius = 0.5f;                  // Sphere/Capsule用
    float height = 1.0f;                  // Capsule用

};