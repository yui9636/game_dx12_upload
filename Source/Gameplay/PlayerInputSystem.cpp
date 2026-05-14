#include "PlayerInputSystem.h"
#include "PlayerTagComponent.h"
#include "LocomotionStateComponent.h"
#include "ActionStateComponent.h"
#include "StateMachineParamsComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Input/InputActionMapComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

namespace
{
    // PlayerEditor 保存と GameLayer シーン読み込みの間で InputActionMap の順序が変わっても壊れないよう、
    // action 名で解決する。index ベースの lookup では、prefab に旧 "LightAttack" が
    // "Dodge" より前に残っていた場合に Dodge が別 slot へずれ、
    // Space が反応しないように見えていた。
    float ReadPressedTriggerByName(
        const ResolvedInputStateComponent& input,
        const InputActionMapComponent& map,
        const char* actionName)
    {
        const auto& actions = map.asset.actions;
        const int count = static_cast<int>(actions.size()) < input.actionCount
            ? static_cast<int>(actions.size())
            : input.actionCount;
        for (int i = 0; i < count; ++i) {
            if (actions[i].actionName == actionName) {
                return input.actions[i].pressed ? 1.0f : 0.0f;
            }
        }
        return 0.0f;
    }

    void ClearActionTriggers(StateMachineParamsComponent& params)
    {
        params.SetParam("Attack", 0.0f);
        params.SetParam("Dodge", 0.0f);
    }
}

void PlayerInputSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<
        PlayerTagComponent,
        LocomotionStateComponent,
        ActionStateComponent,
        ResolvedInputStateComponent,
        InputActionMapComponent,
        StateMachineParamsComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* locoCol = arch->GetColumn(TypeManager::GetComponentTypeID<LocomotionStateComponent>());
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* inputCol = arch->GetColumn(TypeManager::GetComponentTypeID<ResolvedInputStateComponent>());
        auto* mapCol = arch->GetColumn(TypeManager::GetComponentTypeID<InputActionMapComponent>());
        auto* paramsCol = arch->GetColumn(TypeManager::GetComponentTypeID<StateMachineParamsComponent>());
        if (!locoCol || !actionCol || !inputCol || !mapCol || !paramsCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& loco = *static_cast<LocomotionStateComponent*>(locoCol->Get(i));
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& input = *static_cast<ResolvedInputStateComponent*>(inputCol->Get(i));
            auto& map = *static_cast<InputActionMapComponent*>(mapCol->Get(i));
            auto& params = *static_cast<StateMachineParamsComponent*>(paramsCol->Get(i));

            if (action.state == CharacterState::Dead || action.state == CharacterState::Damage) {
                loco.moveInput = { 0.0f, 0.0f };
                ClearActionTriggers(params);
                continue;
            }

            loco.moveInput = { input.axes[0], input.axes[1] };

            // action と dodge の遷移は StateMachine が持つ。Input は 1 フレーム trigger だけを書く。
            // Damaged trigger は HealthSystem が書くので、ここでは触らない。
            params.SetParam("Attack", ReadPressedTriggerByName(input, map, "Attack"));
            params.SetParam("Dodge",  ReadPressedTriggerByName(input, map, "Dodge"));
        }
    }
}
