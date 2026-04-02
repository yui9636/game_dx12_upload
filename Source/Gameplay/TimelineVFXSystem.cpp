#include "TimelineVFXSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "Component/TransformComponent.h"
#include "Effect/EffectManager.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

void TimelineVFXSystem::Update(Registry& registry) {
    Signature sig = CreateSignature<TimelineComponent, TimelineItemBuffer>();
    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* tlCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineComponent>());
        auto* bufCol = arch->GetColumn(TypeManager::GetComponentTypeID<TimelineItemBuffer>());
        auto* txCol  = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!tlCol || !bufCol) continue;

        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            auto& tl  = *static_cast<TimelineComponent*>(tlCol->Get(i));
            auto& buf = *static_cast<TimelineItemBuffer*>(bufCol->Get(i));

            DirectX::XMFLOAT3 worldPos = { 0, 0, 0 };
            if (txCol) {
                auto& tx = *static_cast<TransformComponent*>(txCol->Get(i));
                worldPos = tx.worldPosition;
            }

            for (auto& item : buf.items) {
                if (item.type != 2) continue;
                bool inside = tl.currentFrame >= item.start && tl.currentFrame <= item.end;

                if (inside && !item.vfxActive) {
                    // Spawn VFX
                    DirectX::XMFLOAT3 spawnPos = {
                        worldPos.x + item.vfx.offsetLocal.x,
                        worldPos.y + item.vfx.offsetLocal.y,
                        worldPos.z + item.vfx.offsetLocal.z
                    };
                    item.vfxInstance = EffectManager::Get().Play(item.vfx.assetId, spawnPos);
                    if (item.vfxInstance) {
                        item.vfxInstance->isSequencerControlled = true;
                        item.vfxInstance->loop = !item.vfx.fireOnEnterOnly;
                        float duration = (tl.fps > 0.0f)
                            ? static_cast<float>(item.end - item.start) / tl.fps
                            : 2.0f;
                        item.vfxInstance->lifeTime = duration;
                    }
                    item.vfxActive = true;
                }
                else if (inside && item.vfxActive && item.vfxInstance) {
                    // Sync effect time to timeline
                    float effectAge = (tl.fps > 0.0f)
                        ? static_cast<float>(tl.currentFrame - item.start) / tl.fps
                        : 0.0f;
                    EffectManager::Get().SyncInstanceToTime(item.vfxInstance, effectAge);

                    // Update parent transform
                    DirectX::XMFLOAT3 pos = {
                        worldPos.x + item.vfx.offsetLocal.x,
                        worldPos.y + item.vfx.offsetLocal.y,
                        worldPos.z + item.vfx.offsetLocal.z
                    };
                    DirectX::XMStoreFloat4x4(&item.vfxInstance->parentMatrix,
                        DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z));
                }
                else if (!inside && item.vfxActive) {
                    // Stop VFX
                    if (item.vfxInstance) {
                        item.vfxInstance->Stop(true);
                        item.vfxInstance.reset();
                    }
                    item.vfxActive = false;
                }
            }
        }
    }
}
