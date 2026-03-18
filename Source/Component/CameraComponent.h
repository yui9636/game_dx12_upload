#pragma once
#include <DirectXMath.h>

/**
 * @brief カメラのレンズ性能（光学データ）
 */
struct CameraLensComponent {
    float fovY = 0.785398f; // 45度 (ラジアン)
    float nearZ = 0.1f;
    float farZ = 100000.0f;
    float aspect = 16.0f / 9.0f;
};

/**
 * @brief システムによって書き込まれる行列データ
 * RenderPass はこのコンポーネントを読み取って描画を行う
 */
struct CameraMatricesComponent {
    DirectX::XMFLOAT4X4 view = {};
    DirectX::XMFLOAT4X4 projection = {};
    DirectX::XMFLOAT3   worldPos = { 0, 0, 0 };
    DirectX::XMFLOAT3   cameraFront = { 0, 0, 1 };
};

/**
 * @brief メインカメラ識別用タグ
 */
struct  CameraMainTagComponent {};