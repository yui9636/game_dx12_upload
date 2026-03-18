#pragma once
#include <DirectXMath.h>

struct GridComponent {
    int subdivisions = 20;     // 分割数
    float scale = 1.0f;        // スケール
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // グリッドの色
    bool enabled = true;       // 表示フラグ
};