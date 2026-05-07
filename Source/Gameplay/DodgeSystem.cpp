#include "DodgeSystem.h"
#include "Animator/AnimatorComponent.h"
#include "DodgeStateComponent.h"
#include "ActionStateComponent.h"
#include "CharacterPhysicsComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <cmath>

void DodgeSystem::Update(Registry& registry, float dt) {
    if (dt <= 0.0f) return;

    Signature sig = CreateSignature<DodgeStateComponent, ActionStateComponent, CharacterPhysicsComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* dodgeCol = arch->GetColumn(TypeManager::GetComponentTypeID<DodgeStateComponent>());
        auto* actionCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActionStateComponent>());
        auto* physCol = arch->GetColumn(TypeManager::GetComponentTypeID<CharacterPhysicsComponent>());
        const auto animatorType = TypeManager::GetComponentTypeID<AnimatorComponent>();
        auto* animatorCol = arch->GetSignature().test(animatorType) ? arch->GetColumn(animatorType) : nullptr;
        if (!dodgeCol || !actionCol || !physCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& dodge = *static_cast<DodgeStateComponent*>(dodgeCol->Get(i));
            auto& action = *static_cast<ActionStateComponent*>(actionCol->Get(i));
            auto& phys = *static_cast<CharacterPhysicsComponent*>(physCol->Get(i));
            auto* animator = animatorCol ? static_cast<AnimatorComponent*>(animatorCol->Get(i)) : nullptr;

            // StateMachine owns dodge entry/exit. This system only applies dodge movement while mirrored.
            dodge.dodgeTriggered = false;

            if (action.state != CharacterState::Dodge) {
                dodge.dodgeTimer = 0.0f;
                continue;
            }

            dodge.dodgeTimer += dt;
            if (animator && animator->enableRootMotion) {
                phys.velocity.x = 0.0f;
                phys.velocity.z = 0.0f;
                continue;
            }

            phys.velocity.x = sinf(dodge.dodgeAngleY) * dodge.dodgeMoveSpeed;
            phys.velocity.z = cosf(dodge.dodgeAngleY) * dodge.dodgeMoveSpeed;
        }
    }
}
