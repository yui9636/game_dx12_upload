#include "StaminaSystem.h"
#include "StaminaComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

void StaminaSystem::Update(Registry& registry, float dt) {
    Signature sig = CreateSignature<StaminaComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<StaminaComponent>());
        if (!col) continue;
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& s = *static_cast<StaminaComponent*>(col->Get(i));
            if (s.recoveryTimer > 0.0f) {
                s.recoveryTimer -= dt;
            } else if (s.current < s.max) {
                s.current += s.recoveryRate * dt;
                if (s.current > s.max) s.current = s.max;
            }
        }
    }
}
