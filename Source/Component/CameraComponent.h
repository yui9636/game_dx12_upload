#pragma once
#include <DirectXMath.h>

/**
 * @brief ・ｽJ・ｽ・ｽ・ｽ・ｽ・ｽﾌ・ｿｽ・ｽ・ｽ・ｽY・ｽ・ｽ・ｽ\・ｽi・ｽ・ｽ・ｽw・ｽf・ｽ[・ｽ^・ｽj
 */
struct CameraLensComponent {
    float fovY = 0.785398f;
    float nearZ = 0.1f;
    float farZ = 100000.0f;
    float aspect = 16.0f / 9.0f;
};

/**
 * @brief ・ｽV・ｽX・ｽe・ｽ・ｽ・ｽﾉゑｿｽ・ｽ・ｽﾄ擾ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽﾜゑｿｽ・ｽs・ｽ・ｽf・ｽ[・ｽ^
 * RenderPass ・ｽﾍゑｿｽ・ｽﾌコ・ｽ・ｽ・ｽ|・ｽ[・ｽl・ｽ・ｽ・ｽg・ｽ・ｽﾇみ趣ｿｽ・ｽ・ｽﾄ描・ｽ・ｽ・ｽs・ｽ・ｽ
 */
struct CameraMatricesComponent {
    DirectX::XMFLOAT4X4 view = {};
    DirectX::XMFLOAT4X4 projection = {};
    DirectX::XMFLOAT3   worldPos = { 0, 0, 0 };
    DirectX::XMFLOAT3   cameraFront = { 0, 0, 1 };
};

/**
 * @brief ・ｽ・ｽ・ｽC・ｽ・ｽ・ｽJ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾊ用・ｽ^・ｽO
 */
struct  CameraMainTagComponent {};
