#include "LocomotionSystem.h"
#include "LocomotionStateComponent.h"
#include "ActionStateComponent.h"
#include "CharacterPhysicsComponent.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cmath>
#include <algorithm>
#include <DirectXMath.h>

static constexpr float kPi = 3.14159265f;

// Wrap angle to [-PI, PI]
static float WrapAngle(float a) {
    while (a > kPi)  a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

void LocomotionSystem::Update(Registry& registry, float dt) {
    if (dt <= 0.0f) return;

    Signature sig = CreateSignature<LocomotionStateComponent, ActionStateComponent, CharacterPhysicsComponent, TransformComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* locoCol   = arch->GetColumn(TypeManager::GetComponentTypeID<LocomotionStateComponent>());
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* physCol   = arch->GetColumn(TypeManager::GetComponentTypeID<CharacterPhysicsComponent>());
        auto* transCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!locoCol || !actionCol || !physCol || !transCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& loco   = *static_cast<LocomotionStateComponent*>(locoCol->Get(i));
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& phys   = *static_cast<CharacterPhysicsComponent*>(physCol->Get(i));
            auto& trans  = *static_cast<TransformComponent*>(transCol->Get(i));

            // Only process during Locomotion state
            if (action.state != CharacterState::Locomotion) continue;

            float mx = loco.moveInput.x;
            float my = loco.moveInput.y;
            float strength = sqrtf(mx * mx + my * my);
            strength = std::clamp(strength, 0.0f, 1.0f);
            loco.inputStrength = strength;

            // Compute world move direction (assume input is camera-relative)
            if (strength > 0.001f) {
                loco.worldMoveDir = { mx / strength, my / strength };
            }

            // --- Gait transitions (hysteresis) ---
            uint8_t g = loco.gaitIndex;
            if (g == 0 && strength >= loco.walkThreshold) g = 1;
            if (g == 1 && strength >= loco.jogThreshold)  g = 2;
            if (g == 2 && strength >= loco.runThreshold)   g = 3;
            if (g == 3 && strength < loco.runThreshold - 0.05f) g = 2;
            if (g == 2 && strength < loco.jogThreshold - 0.05f) g = 1;
            if (g == 1 && strength < loco.walkThreshold - 0.02f) g = 0;
            loco.gaitIndex = g;

            // --- Target speed ---
            float targetSpeed = 0.0f;
            switch (g) {
                case 1: targetSpeed = loco.walkMaxSpeed * strength; break;
                case 2: targetSpeed = loco.jogMaxSpeed * strength;  break;
                case 3: targetSpeed = loco.runMaxSpeed * strength;  break;
            }

            // --- Accelerate / decelerate ---
            if (loco.currentSpeed < targetSpeed) {
                float acc = loco.acceleration;
                if (loco.currentSpeed < loco.runMaxSpeed * 0.3f)
                    acc *= loco.launchBoost;
                loco.currentSpeed = std::min(loco.currentSpeed + acc * dt, targetSpeed);
            } else {
                loco.currentSpeed = std::max(loco.currentSpeed - loco.deceleration * dt, 0.0f);
            }

            // --- Rotation ---
            if (strength > 0.001f) {
                float targetAngle = atan2f(loco.worldMoveDir.x, loco.worldMoveDir.y);
                loco.targetAngleY = targetAngle;

                // Current facing from quaternion
                using namespace DirectX;
                XMVECTOR q = XMLoadFloat4(&trans.localRotation);
                XMVECTOR fwd = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
                float currentAngle = atan2f(XMVectorGetX(fwd), XMVectorGetZ(fwd));

                float diff = WrapAngle(targetAngle - currentAngle);
                float maxTurn = loco.turnSpeed * (kPi / 180.0f) * dt;

                float newAngle;
                if (fabsf(diff) <= maxTurn) {
                    newAngle = targetAngle;
                    loco.turningInPlace = false;
                } else {
                    newAngle = currentAngle + (diff > 0 ? maxTurn : -maxTurn);
                    loco.turningInPlace = (loco.currentSpeed < 1.0f);
                    loco.lastTurnSign = (diff > 0) ? 1 : -1;
                }

                XMVECTOR newQuat = XMQuaternionRotationRollPitchYaw(0, newAngle, 0);
                XMStoreFloat4(&trans.localRotation, newQuat);

                // Move in the requested direction while the visual facing catches up.
                phys.velocity.x = sinf(targetAngle) * loco.currentSpeed;
                phys.velocity.z = cosf(targetAngle) * loco.currentSpeed;
            } else {
                // No input — decelerate to stop
                loco.turningInPlace = false;
                loco.worldMoveDir = { 0.0f, 0.0f };
                if (loco.currentSpeed > 0.0f) {
                    loco.currentSpeed = std::max(loco.currentSpeed - loco.deceleration * dt, 0.0f);
                }
                if (loco.currentSpeed < 0.01f) {
                    phys.velocity.x = 0.0f;
                    phys.velocity.z = 0.0f;
                    loco.currentSpeed = 0.0f;
                    loco.gaitIndex = 0;
                }
            }
        }
    }
}
