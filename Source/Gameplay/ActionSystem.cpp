#include "ActionSystem.h"
#include "ActionStateComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

void ActionSystem::Update(Registry& registry, float dt) {
    Signature sig = CreateSignature<ActionStateComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        if (!actionCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));

            // StateMachine owns action entry/exit. This system only keeps legacy combo timers sane.
            if (action.state == CharacterState::Locomotion && action.comboCount > 0) {
                action.comboTimer += dt;
                if (action.comboTimer >= action.comboTimeout) {
                    action.comboCount = 0;
                    action.comboTimer = 0.0f;
                }
            } else if (action.state != CharacterState::Locomotion) {
                action.comboTimer = 0.0f;
            }
        }
    }
}
