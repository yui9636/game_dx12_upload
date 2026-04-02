#include "PlayerInputSystem.h"
#include "PlayerTagComponent.h"
#include "LocomotionStateComponent.h"
#include "ActionStateComponent.h"
#include "DodgeStateComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

namespace InputAction {
    constexpr int AttackLight = 0;
    constexpr int AttackHeavy = 1;
    constexpr int Dodge = 2;
}

void PlayerInputSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<PlayerTagComponent, LocomotionStateComponent, ActionStateComponent, ResolvedInputStateComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* locoCol   = arch->GetColumn(TypeManager::GetComponentTypeID<LocomotionStateComponent>());
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* inputCol  = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        auto* dodgeCol  = arch->GetColumn(TypeManager::GetComponentTypeID<DodgeStateComponent>());
        if (!locoCol || !actionCol || !inputCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& loco   = *static_cast<LocomotionStateComponent*>(locoCol->Get(i));
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& input  = *static_cast<ResolvedInputStateComponent*>(inputCol->Get(i));

            // Dead/Damage states ignore input
            if (action.state == CharacterState::Dead || action.state == CharacterState::Damage)
                continue;

            // Move input (always written)
            loco.moveInput = { input.axes[0], input.axes[1] };

            // Dodge trigger
            if (dodgeCol) {
                auto& dodge = *static_cast<DodgeStateComponent*>(dodgeCol->Get(i));
                dodge.dodgeTriggered = false;
                if (input.actions[InputAction::Dodge].pressed &&
                    input.actions[InputAction::Dodge].framesSincePressed < 10) {
                    dodge.dodgeTriggered = true;
                }
            }

            // Attack triggers — only from Locomotion
            if (action.state == CharacterState::Locomotion) {
                if (input.actions[InputAction::AttackLight].pressed &&
                    input.actions[InputAction::AttackLight].framesSincePressed < 10) {
                    action.reservedNodeIndex = (loco.gaitIndex >= 2) ? 10 : 0;
                }
                else if (input.actions[InputAction::AttackHeavy].pressed &&
                         input.actions[InputAction::AttackHeavy].framesSincePressed < 12) {
                    action.reservedNodeIndex = 7;
                }
            }
            // Combo input buffering during Action is handled by ActionSystem
        }
    }
}
