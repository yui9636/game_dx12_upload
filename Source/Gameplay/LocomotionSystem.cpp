#include "LocomotionSystem.h"
#include "LocomotionStateComponent.h"
#include "ActionStateComponent.h"
#include "Component/CameraComponent.h"
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

struct PlanarCameraBasis
{
    DirectX::XMFLOAT2 right = { 1.0f, 0.0f };
    DirectX::XMFLOAT2 forward = { 0.0f, 1.0f };
};

static PlanarCameraBasis ResolveMainCameraBasis(Registry& registry)
{
    using namespace DirectX;

    PlanarCameraBasis basis;
    Signature sig = CreateSignature<TransformComponent, CameraMainTagComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* transCol = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!transCol || arch->GetEntityCount() == 0) continue;

        auto& trans = *static_cast<TransformComponent*>(transCol->Get(0));
        XMMATRIX W = XMLoadFloat4x4(&trans.worldMatrix);

        XMFLOAT3 right3{};
        XMFLOAT3 forward3{};
        XMStoreFloat3(&right3, XMVector3Normalize(W.r[0]));
        XMStoreFloat3(&forward3, XMVector3Normalize(W.r[2]));

        float rightLen = sqrtf(right3.x * right3.x + right3.z * right3.z);
        float forwardLen = sqrtf(forward3.x * forward3.x + forward3.z * forward3.z);
        if (rightLen > 0.001f) {
            basis.right = { right3.x / rightLen, right3.z / rightLen };
        }
        if (forwardLen > 0.001f) {
            basis.forward = { forward3.x / forwardLen, forward3.z / forwardLen };
        }
        break;
    }

    return basis;
}

void LocomotionSystem::Update(Registry& registry, float dt) {
    if (dt <= 0.0f) return;
    const PlanarCameraBasis cameraBasis = ResolveMainCameraBasis(registry);

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

            if (action.state != CharacterState::Locomotion) continue;

            const float inputX = loco.moveInput.x;
            const float inputY = loco.moveInput.y;
            // AI / scripted entities (useCameraRelativeInput == false) write moveInput
            // already in world x/z. Player entities use camera-relative stick input.
            const float worldX = loco.useCameraRelativeInput
                ? (cameraBasis.right.x * inputX + cameraBasis.forward.x * inputY)
                : inputX;
            const float worldZ = loco.useCameraRelativeInput
                ? (cameraBasis.right.y * inputX + cameraBasis.forward.y * inputY)
                : inputY;

            float strength = sqrtf(worldX * worldX + worldZ * worldZ);
            strength = std::clamp(strength, 0.0f, 1.0f);
            loco.inputStrength = strength;

            if (strength > 0.001f) {
                loco.worldMoveDir = { worldX / strength, worldZ / strength };
            }

            uint8_t g = loco.gaitIndex;
            if (g == 0 && strength >= loco.walkThreshold) g = 1;
            if (g == 1 && strength >= loco.jogThreshold)  g = 2;
            if (g == 2 && strength >= loco.runThreshold)  g = 3;
            if (g == 3 && strength < loco.runThreshold - 0.05f) g = 2;
            if (g == 2 && strength < loco.jogThreshold - 0.05f) g = 1;
            if (g == 1 && strength < loco.walkThreshold - 0.02f) g = 0;
            loco.gaitIndex = g;

            float targetSpeed = 0.0f;
            switch (g) {
            case 1: targetSpeed = loco.walkMaxSpeed * strength; break;
            case 2: targetSpeed = loco.jogMaxSpeed * strength; break;
            case 3: targetSpeed = loco.runMaxSpeed * strength; break;
            }

            if (loco.currentSpeed < targetSpeed) {
                float acc = loco.acceleration;
                if (loco.currentSpeed < loco.runMaxSpeed * 0.3f) {
                    acc *= loco.launchBoost;
                }
                loco.currentSpeed = std::min(loco.currentSpeed + acc * dt, targetSpeed);
            } else {
                loco.currentSpeed = std::max(loco.currentSpeed - loco.deceleration * dt, 0.0f);
            }

            if (strength > 0.001f) {
                using namespace DirectX;
                const float targetAngle = atan2f(loco.worldMoveDir.x, loco.worldMoveDir.y);
                loco.targetAngleY = targetAngle;

                XMVECTOR q = XMLoadFloat4(&trans.localRotation);
                XMVECTOR fwd = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
                const float currentAngle = atan2f(XMVectorGetX(fwd), XMVectorGetZ(fwd));
                const float diff = WrapAngle(targetAngle - currentAngle);

                const float maxTurn = loco.turnSpeed * (kPi / 180.0f) * dt;
                float newAngle = targetAngle;
                if (fabsf(diff) > maxTurn) {
                    newAngle = currentAngle + (diff > 0.0f ? maxTurn : -maxTurn);
                }

                loco.turningInPlace = false;
                loco.lastTurnSign = 0;
                if (fabsf(diff) > 0.001f) {
                    loco.lastTurnSign = (diff > 0.0f) ? 1 : -1;
                }

                XMVECTOR newQuat = XMQuaternionRotationRollPitchYaw(0, newAngle, 0);
                XMStoreFloat4(&trans.localRotation, newQuat);

                phys.velocity.x = loco.worldMoveDir.x * loco.currentSpeed;
                phys.velocity.z = loco.worldMoveDir.y * loco.currentSpeed;
            } else {
                loco.worldMoveDir = { 0.0f, 0.0f };
                loco.turningInPlace = false;
                loco.lastTurnSign = 0;
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
