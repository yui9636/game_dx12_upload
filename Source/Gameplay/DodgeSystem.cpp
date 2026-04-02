#include "DodgeSystem.h"
#include "DodgeStateComponent.h"
#include "ActionStateComponent.h"
#include "ActionDatabaseComponent.h"
#include "StaminaComponent.h"
#include "CharacterPhysicsComponent.h"
#include "LocomotionStateComponent.h"
#include "PlaybackComponent.h"
#include "Component/TransformComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cmath>

void DodgeSystem::Update(Registry& registry, float dt) {
    Signature sig = CreateSignature<DodgeStateComponent, ActionStateComponent, StaminaComponent, CharacterPhysicsComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* dodgeCol   = arch->GetColumn(TypeManager::GetComponentTypeID<DodgeStateComponent>());
        auto* actionCol  = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* staminaCol = arch->GetColumn(TypeManager::GetComponentTypeID<StaminaComponent>());
        auto* physCol    = arch->GetColumn(TypeManager::GetComponentTypeID<CharacterPhysicsComponent>());
        auto* dbCol      = arch->GetColumn(TypeManager::GetComponentTypeID<ActionDatabaseComponent>());
        auto* playCol    = arch->GetColumn(TypeManager::GetComponentTypeID<PlaybackComponent>());
        auto* locoCol    = arch->GetColumn(TypeManager::GetComponentTypeID<LocomotionStateComponent>());
        auto* transCol   = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!dodgeCol || !actionCol || !staminaCol || !physCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& dodge   = *static_cast<DodgeStateComponent*>(dodgeCol->Get(i));
            auto& action  = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& stamina = *static_cast<StaminaComponent*>(staminaCol->Get(i));
            auto& phys    = *static_cast<CharacterPhysicsComponent*>(physCol->Get(i));

            // --- Dodge trigger ---
            if (dodge.dodgeTriggered && action.state != CharacterState::Dodge &&
                action.state != CharacterState::Dead && action.state != CharacterState::Damage) {

                // If in Action state, check cancel window
                if (action.state == CharacterState::Action && dbCol && playCol) {
                    auto& db = *static_cast<ActionDatabaseComponent*>(dbCol->Get(i));
                    auto& pb = *static_cast<PlaybackComponent*>(playCol->Get(i));
                    if (action.currentNodeIndex >= 0 && action.currentNodeIndex < db.nodeCount) {
                        float t01 = (pb.clipLength > 0.0f) ? pb.currentSeconds / pb.clipLength : 1.0f;
                        if (t01 < db.nodes[action.currentNodeIndex].cancelStart) {
                            dodge.dodgeTriggered = false;
                            goto skip_dodge; // Not yet in cancel window
                        }
                    }
                }

                // Stamina check
                if (stamina.current < stamina.costPerUse) {
                    dodge.dodgeTriggered = false;
                    goto skip_dodge;
                }

                // Consume stamina
                stamina.current -= stamina.costPerUse;
                stamina.recoveryTimer = stamina.recoveryDelay;

                // Determine dodge direction from move input
                float angleY = 0.0f;
                if (locoCol) {
                    auto& loco = *static_cast<LocomotionStateComponent*>(locoCol->Get(i));
                    float mx = loco.moveInput.x, my = loco.moveInput.y;
                    float len = sqrtf(mx * mx + my * my);
                    if (len > 0.01f) {
                        angleY = atan2f(mx, my);
                    } else if (transCol) {
                        // No input: dodge backward (opposite facing)
                        auto& tx = *static_cast<TransformComponent*>(transCol->Get(i));
                        DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&tx.localRotation);
                        DirectX::XMVECTOR fwd = DirectX::XMVector3Rotate(
                            DirectX::XMVectorSet(0, 0, 1, 0), q);
                        angleY = atan2f(DirectX::XMVectorGetX(fwd), DirectX::XMVectorGetZ(fwd))
                                 + 3.14159265f; // Backward
                    }
                }

                // Start dodge
                dodge.dodgeAngleY = angleY;
                dodge.dodgeTimer = 0.0f;
                action.state = CharacterState::Dodge;
                action.currentNodeIndex = -1;
                action.reservedNodeIndex = -1;

                // Rotate to dodge direction
                if (transCol) {
                    auto& tx = *static_cast<TransformComponent*>(transCol->Get(i));
                    DirectX::XMVECTOR newQ = DirectX::XMQuaternionRotationRollPitchYaw(0, angleY, 0);
                    DirectX::XMStoreFloat4(&tx.localRotation, newQ);
                }

                dodge.dodgeTriggered = false;
            }

            skip_dodge:

            // --- Dodge movement ---
            if (action.state == CharacterState::Dodge) {
                float speed = dodge.dodgeMoveSpeed;
                phys.velocity.x = sinf(dodge.dodgeAngleY) * speed;
                phys.velocity.z = cosf(dodge.dodgeAngleY) * speed;

                dodge.dodgeTimer += dt;
                float exitTime = dodge.dodgeDuration * dodge.dodgeExitNormalized;
                if (dodge.dodgeTimer >= exitTime) {
                    action.state = CharacterState::Locomotion;
                    phys.velocity.x = 0.0f;
                    phys.velocity.z = 0.0f;
                }
            }
        }
    }
}
