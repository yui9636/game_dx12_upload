#include "PlayerInputSystem.h"
#include "PlayerTagComponent.h"
#include "LocomotionStateComponent.h"
#include "ActionStateComponent.h"
#include "StateMachineParamsComponent.h"
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

namespace
{
    float ReadPressedTrigger(const ResolvedInputStateComponent& input, int actionIndex)
    {
        if (actionIndex < 0 || actionIndex >= input.actionCount) {
            return 0.0f;
        }
        return input.actions[actionIndex].pressed ? 1.0f : 0.0f;
    }

    void ClearActionTriggers(StateMachineParamsComponent& params)
    {
        params.SetParam("LightAttack", 0.0f);
        params.SetParam("HeavyAttack", 0.0f);
        params.SetParam("Dodge", 0.0f);
    }
}

void PlayerInputSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<
        PlayerTagComponent,
        LocomotionStateComponent,
        ActionStateComponent,
        ResolvedInputStateComponent,
        StateMachineParamsComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* locoCol = arch->GetColumn(TypeManager::GetComponentTypeID<LocomotionStateComponent>());
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* inputCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        auto* paramsCol = arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineParamsComponent>());
        if (!locoCol || !actionCol || !inputCol || !paramsCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& loco = *static_cast<LocomotionStateComponent*>(locoCol->Get(i));
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& input = *static_cast<ResolvedInputStateComponent*>(inputCol->Get(i));
            auto& params = *static_cast<StateMachineParamsComponent*>(paramsCol->Get(i));

            if (action.state == CharacterState::Dead || action.state == CharacterState::Damage) {
                loco.moveInput = { 0.0f, 0.0f };
                ClearActionTriggers(params);
                continue;
            }

            loco.moveInput = { input.axes[0], input.axes[1] };

            // StateMachine owns action and dodge transitions. Input only writes one-frame triggers.
            params.SetParam("LightAttack", ReadPressedTrigger(input, InputAction::AttackLight));
            params.SetParam("HeavyAttack", ReadPressedTrigger(input, InputAction::AttackHeavy));
            params.SetParam("Dodge", ReadPressedTrigger(input, InputAction::Dodge));
        }
    }
}
