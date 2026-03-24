#pragma once
#include <DirectXMath.h>

/**
 * @brief ・ｽﾄ用・ｽI・ｽﾈデ・ｽo・ｽb・ｽO・ｽ\・ｽ・ｽ・ｽp・ｽR・ｽ・ｽ・ｽ|・ｽ[・ｽl・ｽ・ｽ・ｽg
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
