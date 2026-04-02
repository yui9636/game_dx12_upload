#include "InputContextSystem.h"
#include "InputContextComponent.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"
#include <vector>
#include <algorithm>

void InputContextSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<InputContextComponent>();
    auto archetypes = registry.GetAllArchetypes();

    struct ContextEntry {
        InputContextComponent* ctx;
        InputContextPriority priority;
    };

    std::vector<ContextEntry> entries;

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
        if (!col) continue;
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto* ctx = static_cast<InputContextComponent*>(col->Get(i));
            ctx->consumed = false; // reset each frame
            if (ctx->enabled) {
                entries.push_back({ ctx, ctx->priority });
            }
        }
    }

    // Sort by priority descending (highest priority first)
    std::sort(entries.begin(), entries.end(), [](const ContextEntry& a, const ContextEntry& b) {
        return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
    });

    // Walk sorted list; mark lower-priority contexts as consumed
    bool shouldConsume = false;
    for (auto& entry : entries) {
        if (shouldConsume) {
            entry.ctx->consumed = true;
        }
        if (entry.ctx->consumeLowerPriority) {
            shouldConsume = true;
        }
    }
}
