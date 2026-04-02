#include "HealthSystem.h"
#include "HealthComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

void HealthSystem::Update(Registry& registry, float dt) {
    Signature sig = CreateSignature<HealthComponent>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<HealthComponent>());
        if (!col) continue;
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& h = *static_cast<HealthComponent*>(col->Get(i));
            if (h.invincibleTimer > 0.0f) {
                h.invincibleTimer -= dt;
                if (h.invincibleTimer <= 0.0f) h.isInvincible = false;
            }
            h.isDead = (h.health <= 0);
        }
    }
}
