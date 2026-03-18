#pragma once
#include <vector>
#include <string>
#include <SimpleMath.h>
#include "Collision/Collision.h" //

/**
 * @brief 当たり判定の設定データ
 */
struct ColliderComponent {
    struct Element {
        ColliderShape type = ColliderShape::Sphere; //
        bool enabled = true;
        int nodeIndex = -1; // ボーン追従用
        DirectX::SimpleMath::Vector3 offsetLocal{ 0,0,0 };
        float radius = 0.5f;
        float height = 1.0f;
        DirectX::SimpleMath::Vector3 size{ 1.0f, 1.0f, 1.0f };
        DirectX::SimpleMath::Vector4 color{ 1.0f, 0.0f, 0.0f, 0.35f };
        ColliderAttribute attribute = ColliderAttribute::Body; //

        // 実行時のキャッシュ（Systemが書き込む）
        uint32_t registeredId = 0;
        int runtimeTag = 0; // シーケンサー用
    };

    std::vector<Element> elements;
    bool enabled = true;
    bool drawGizmo = true;

};