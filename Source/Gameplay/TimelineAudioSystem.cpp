#include "TimelineAudioSystem.h"
#include "TimelineComponent.h"
#include "TimelineItemBuffer.h"
#include "Component/TransformComponent.h"
#include "Audio/AudioWorldSystem.h"
#include "Engine/EngineKernel.h"
#include "Registry/Registry.h"
#include "Component/ComponentSignature.h"
#include "Type/TypeInfo.h"
#include "Archetype/Archetype.h"

void TimelineAudioSystem::Update(Registry& registry) {
    auto& audio = EngineKernel::Instance().GetAudioWorld();
    const bool editorPreview = EngineKernel::Instance().GetMode() == EngineMode::Editor;

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
            const EntityID ownerEntity = arch->GetEntities()[i];
            TransformComponent* tx = txCol ? static_cast<TransformComponent*>(txCol->Get(i)) : nullptr;

            for (auto& item : buf.items) {
                if (item.type != 3) continue;
                bool inside = tl.currentFrame >= item.start && tl.currentFrame <= item.end;
                const DirectX::XMFLOAT3 worldPos = tx ? tx->worldPosition : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
                const bool hasClipPath = item.audio.assetId[0] != '\0';

                if (inside && !item.audioActive) {
                    if (!hasClipPath) {
                        continue;
                    }

                    // Play audio
                    if (item.audio.is3D) {
                        item.audioHandle = editorPreview
                            ? audio.PlayEditorTransient3D(
                                item.audio.assetId,
                                worldPos,
                                item.audio.volume,
                                item.audio.pitch,
                                item.audio.loop)
                            : audio.PlayTransient3D(
                                item.audio.assetId,
                                worldPos,
                                item.audio.volume,
                                item.audio.pitch,
                                item.audio.loop);
                    } else {
                        item.audioHandle = editorPreview
                            ? audio.PlayEditorTransient2D(
                                item.audio.assetId,
                                item.audio.volume,
                                item.audio.pitch,
                                item.audio.loop)
                            : audio.PlayTransient2D(
                                item.audio.assetId,
                                item.audio.volume,
                                item.audio.pitch,
                                item.audio.loop);
                    }

                    if (item.audioHandle != 0) {
                        item.audioActive = true;
                    }
                }
                else if (inside && item.audioActive && item.audioHandle != 0 && item.audio.is3D) {
                    // Update 3D position
                    audio.SetVoicePosition(item.audioHandle, worldPos);
                }
                else if (!inside && item.audioActive) {
                    // Stop audio
                    if (item.audioHandle != 0) {
                        audio.StopVoice(item.audioHandle);
                    }
                    item.audioActive = false;
                    item.audioHandle = 0;
                }
            }
        }
    }
}
