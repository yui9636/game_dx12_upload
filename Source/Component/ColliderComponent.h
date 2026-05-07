#pragma once
#include <vector>
#include <string>
#include <SimpleMath.h>
#include "Collision/Collision.h" //

/**
 * @brief コライダー要素の設定データ
 */
struct ColliderComponent {
    struct Element {
        ColliderShape type = ColliderShape::Sphere; //
        bool enabled = true;
        int nodeIndex = -1;
        DirectX::SimpleMath::Vector3 offsetLocal{ 0,0,0 };
        float radius = 0.5f;
        float height = 1.0f;
        DirectX::SimpleMath::Vector3 size{ 1.0f, 1.0f, 1.0f };
        DirectX::SimpleMath::Vector4 color{ 1.0f, 0.0f, 0.0f, 0.35f };
        ColliderAttribute attribute = ColliderAttribute::Body; //

        // Body-only hit feedback. Read by DamageSystem when this element is
        // hit by an Attack collider; HealthSystem then plays the VFX/SE at
        // the contact point. Attack elements ignore these.
        std::string hitVfxPath;
        std::string hitSfxPath;

        uint32_t registeredId = 0;
        int runtimeTag = 0;
    };

    std::vector<Element> elements;
    bool enabled = true;
    bool drawGizmo = true;

};
