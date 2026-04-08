#include "EffectSystems.h"

#include <algorithm>
#include <cmath>

#include "Archetype/Archetype.h"
#include "Component/ComponentSignature.h"
#include "Component/EffectAssetComponent.h"
#include "Component/EffectAttachmentComponent.h"
#include "Component/EffectParameterOverrideComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/EffectSpawnRequestComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "EffectParameterBindings.h"
#include "EffectRuntimeRegistry.h"
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "Model/Model.h"
#include "Model/ModelResource.h"
#include "System/ResourceManager.h"
#include "Type/TypeInfo.h"

namespace
{
    template <typename... Ts>
    bool ArchetypeHas(Archetype* archetype)
    {
        return archetype && SignatureMatches(archetype->GetSignature(), CreateSignature<Ts...>());
    }

    void SyncRuntimeTime(const EffectPlaybackComponent& playback)
    {
        if (playback.runtimeInstanceId == 0) {
            return;
        }

        if (auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback.runtimeInstanceId)) {
            runtime->time = playback.currentTime;
        }
    }

    void UpdateLifetimeFade(EffectPlaybackComponent& playback)
    {
        if (!playback.isPlaying) {
            playback.lifetimeFade = 0.0f;
            return;
        }

        if (playback.duration <= 0.0f || playback.loop) {
            playback.lifetimeFade = 1.0f;
            return;
        }

        const float normalized = std::clamp(playback.currentTime / playback.duration, 0.0f, 1.0f);
        playback.lifetimeFade = 1.0f - normalized;
    }

    void AdvancePlayback(EffectPlaybackComponent& playback, float dt)
    {
        if (!playback.isPlaying || playback.runtimeInstanceId == 0 || dt <= 0.0f) {
            return;
        }

        playback.currentTime += dt;

        if (playback.duration > 0.0f && playback.loop) {
            while (playback.currentTime >= playback.duration) {
                playback.currentTime -= playback.duration;
            }
        }

        SyncRuntimeTime(playback);
        UpdateLifetimeFade(playback);
    }

    void FinalizeStoppedPlayback(EffectPlaybackComponent& playback)
    {
        if (playback.runtimeInstanceId != 0) {
            EffectRuntimeRegistry::Instance().Destroy(playback.runtimeInstanceId);
            playback.runtimeInstanceId = 0;
        }

        playback.isPlaying = false;
        playback.currentTime = playback.duration;
        playback.lifetimeFade = 0.0f;
        SyncRuntimeTime(playback);
    }
}

void EffectSpawnSystem::Update(Registry& registry, float)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectAssetComponent, EffectPlaybackComponent>(archetype)) {
            continue;
        }

        auto* assetColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectAssetComponent>());
        auto* playbackColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectPlaybackComponent>());
        auto* requestColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectSpawnRequestComponent>());
        if (!assetColumn || !playbackColumn) {
            continue;
        }

        auto* assets = static_cast<EffectAssetComponent*>(assetColumn->Get(0));
        auto* playbacks = static_cast<EffectPlaybackComponent*>(playbackColumn->Get(0));
        auto* requests = requestColumn ? static_cast<EffectSpawnRequestComponent*>(requestColumn->Get(0)) : nullptr;

        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            auto& asset = assets[row];
            auto& playback = playbacks[row];
            auto* request = requests ? &requests[row] : nullptr;

            const bool hasPendingRequest = request && request->pending;
            const bool shouldAutoPlay = asset.autoPlay && playback.runtimeInstanceId == 0 && !playback.isPlaying;
            if ((!hasPendingRequest && !shouldAutoPlay) || asset.assetPath.empty()) {
                continue;
            }

            auto compiled = EffectRuntimeRegistry::Instance().GetCompiledAsset(asset.assetPath);
            if (!compiled || !compiled->valid) {
                playback.isPlaying = false;
                continue;
            }

            if (playback.runtimeInstanceId != 0) {
                if (!request || !request->restartIfActive) {
                    continue;
                }
                EffectRuntimeRegistry::Instance().Destroy(playback.runtimeInstanceId);
                playback.runtimeInstanceId = 0;
            }

            const uint32_t runtimeId = EffectRuntimeRegistry::Instance().Spawn(asset.assetPath, playback.seed);
            if (runtimeId == 0) {
                playback.isPlaying = false;
                continue;
            }

            const float requestedStartTime = request
                ? (request->startTime < 0.0f ? 0.0f : request->startTime)
                : (playback.currentTime < 0.0f ? 0.0f : playback.currentTime);
            playback.runtimeInstanceId = runtimeId;
            playback.currentTime = requestedStartTime;
            playback.duration = compiled->duration > 0.0f ? compiled->duration : playback.duration;
            playback.loop = asset.loop || playback.loop;
            playback.isPlaying = true;
            playback.stopRequested = false;
            SyncRuntimeTime(playback);
            UpdateLifetimeFade(playback);

            if (request) {
                request->pending = false;
                request->startTime = 0.0f;
            }
        }
    }
}

void EffectPlaybackSystem::Update(Registry& registry, float dt)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectPlaybackComponent>(archetype)) {
            continue;
        }

        auto* playbackColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectPlaybackComponent>());
        if (!playbackColumn) {
            continue;
        }

        auto* playbacks = static_cast<EffectPlaybackComponent*>(playbackColumn->Get(0));
        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            auto& playback = playbacks[row];
            if (!playback.isPlaying || playback.runtimeInstanceId == 0) {
                continue;
            }

            AdvancePlayback(playback, dt);
        }
    }
}

void EffectAttachmentSystem::Update(Registry& registry)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectAttachmentComponent, TransformComponent>(archetype)) {
            continue;
        }

        auto* attachmentColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectAttachmentComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        if (!attachmentColumn || !transformColumn) {
            continue;
        }

        auto* attachments = static_cast<EffectAttachmentComponent*>(attachmentColumn->Get(0));
        auto* transforms = static_cast<TransformComponent*>(transformColumn->Get(0));

        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            auto& attachment = attachments[row];
            auto& transform = transforms[row];
            if (Entity::IsNull(attachment.parentEntity) || !registry.IsAlive(attachment.parentEntity)) {
                continue;
            }

            auto* parentTransform = registry.GetComponent<TransformComponent>(attachment.parentEntity);
            if (!parentTransform) {
                continue;
            }

            transform.localPosition = {
                parentTransform->worldPosition.x + attachment.offsetLocal.x,
                parentTransform->worldPosition.y + attachment.offsetLocal.y,
                parentTransform->worldPosition.z + attachment.offsetLocal.z
            };
            transform.localScale = attachment.offsetScale;
            transform.isDirty = true;
        }
    }
}

void EffectSimulationSystem::Update(Registry& registry, float)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectPlaybackComponent>(archetype)) {
            continue;
        }

        auto* playbackColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectPlaybackComponent>());
        if (!playbackColumn) {
            continue;
        }

        auto* playbacks = static_cast<EffectPlaybackComponent*>(playbackColumn->Get(0));
        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            UpdateLifetimeFade(playbacks[row]);
        }
    }
}

void EffectLifetimeSystem::Update(Registry& registry, float)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectPlaybackComponent>(archetype)) {
            continue;
        }

        auto* playbackColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectPlaybackComponent>());
        if (!playbackColumn) {
            continue;
        }

        auto* playbacks = static_cast<EffectPlaybackComponent*>(playbackColumn->Get(0));
        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            auto& playback = playbacks[row];
            const bool reachedEnd = playback.duration > 0.0f && playback.currentTime >= playback.duration && !playback.loop;
            if (!playback.isPlaying || (!playback.stopRequested && !reachedEnd)) {
                continue;
            }

            FinalizeStoppedPlayback(playback);
        }
    }
}

void EffectPreviewSystem::Update(Registry& registry, float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectPreviewTagComponent, EffectPlaybackComponent>(archetype)) {
            continue;
        }

        auto* playbackColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectPlaybackComponent>());
        if (!playbackColumn) {
            continue;
        }

        auto* playbacks = static_cast<EffectPlaybackComponent*>(playbackColumn->Get(0));
        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            auto& playback = playbacks[row];
            if (!playback.loop && playback.isPlaying && playback.duration <= 0.0f) {
                playback.loop = true;
            }

            AdvancePlayback(playback, dt);

            const bool reachedEnd = playback.duration > 0.0f && playback.currentTime >= playback.duration && !playback.loop;
            if (playback.isPlaying && (playback.stopRequested || reachedEnd)) {
                FinalizeStoppedPlayback(playback);
            }
        }
    }
}

void EffectExtractSystem::Extract(Registry& registry, RenderContext& rc, RenderQueue& queue)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!ArchetypeHas<EffectAssetComponent, EffectPlaybackComponent, TransformComponent>(archetype)) {
            continue;
        }

        auto* assetColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectAssetComponent>());
        auto* playbackColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectPlaybackComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
        auto* materialColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MaterialComponent>());
        auto* overrideColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<EffectParameterOverrideComponent>());
        if (!assetColumn || !playbackColumn || !transformColumn) {
            continue;
        }

        auto* assets = static_cast<EffectAssetComponent*>(assetColumn->Get(0));
        auto* playbacks = static_cast<EffectPlaybackComponent*>(playbackColumn->Get(0));
        auto* transforms = static_cast<TransformComponent*>(transformColumn->Get(0));
        auto* meshes = meshColumn ? static_cast<MeshComponent*>(meshColumn->Get(0)) : nullptr;
        auto* materials = materialColumn ? static_cast<MaterialComponent*>(materialColumn->Get(0)) : nullptr;
        auto* overrides = overrideColumn ? static_cast<EffectParameterOverrideComponent*>(overrideColumn->Get(0)) : nullptr;

        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            auto& asset = assets[row];
            auto& playback = playbacks[row];
            auto& transform = transforms[row];
            if (!playback.isPlaying || playback.runtimeInstanceId == 0 || asset.assetPath.empty()) {
                continue;
            }

            auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback.runtimeInstanceId);
            if (!runtime || !runtime->compiledAsset || !runtime->compiledAsset->valid) {
                continue;
            }

            const auto& compiled = *runtime->compiledAsset;
            EffectMeshRendererDescriptor effectiveMesh = compiled.meshRenderer;
            EffectParticleSimulationLayout effectiveParticle = compiled.particleRenderer;
            float effectiveDuration = compiled.duration > 0.0f ? compiled.duration : playback.duration;

            if (overrides && overrides[row].enabled) {
                if (!overrides[row].scalarParameter.empty()) {
                    EffectParameterBindings::ApplyFloatParameter(
                        overrides[row].scalarParameter,
                        overrides[row].scalarValue,
                        effectiveMesh,
                        effectiveParticle,
                        effectiveDuration);
                }
                if (!overrides[row].colorParameter.empty()) {
                    EffectParameterBindings::ApplyColorParameter(
                        overrides[row].colorParameter,
                        overrides[row].colorValue,
                        effectiveMesh,
                        effectiveParticle);
                }
            }

            DirectX::XMFLOAT4 tint = effectiveMesh.tint;
            tint.w *= playback.lifetimeFade;

            if (effectiveMesh.enabled) {
                std::shared_ptr<ModelResource> modelResource;
                if (!effectiveMesh.meshAssetPath.empty()) {
                    if (auto model = ResourceManager::Instance().GetModel(effectiveMesh.meshAssetPath)) {
                        modelResource = model->GetModelResource();
                    }
                } else if (meshes && meshes[row].model) {
                    modelResource = meshes[row].model->GetModelResource();
                }

                std::shared_ptr<MaterialAsset> materialAsset;
                if (!effectiveMesh.materialAssetPath.empty()) {
                    materialAsset = ResourceManager::Instance().GetMaterial(effectiveMesh.materialAssetPath);
                } else if (materials && !materials[row].materialAssetPath.empty()) {
                    materialAsset = materials[row].materialAsset
                        ? materials[row].materialAsset
                        : ResourceManager::Instance().GetMaterial(materials[row].materialAssetPath);
                }

                if (modelResource) {
                    EffectMeshPacket packet;
                    packet.modelResource = modelResource;
                    packet.worldMatrix = transform.worldMatrix;
                    packet.prevWorldMatrix = transform.prevWorldMatrix;
                    packet.shaderId = effectiveMesh.shaderId;
                    packet.baseColor = tint;
                    packet.materialAsset = materialAsset;
                    packet.blendState = effectiveMesh.blendState;
                    packet.depthState = effectiveMesh.depthState;
                    packet.rasterizerState = effectiveMesh.rasterizerState;
                    packet.shaderVariantKey = effectiveMesh.shaderVariantKey;
                    packet.lifetimeFade = playback.lifetimeFade;
                    const float dx = transform.worldPosition.x - rc.cameraPosition.x;
                    const float dy = transform.worldPosition.y - rc.cameraPosition.y;
                    const float dz = transform.worldPosition.z - rc.cameraPosition.z;
                    packet.distanceToCamera = std::sqrt(dx * dx + dy * dy + dz * dz);
                    packet.sortKey = static_cast<uint64_t>(packet.distanceToCamera * 1000.0f);
                    queue.effectMeshPackets.push_back(std::move(packet));
                }
            }

            if (effectiveParticle.enabled) {
                std::shared_ptr<ModelResource> particleModelResource;
                if (effectiveParticle.drawMode == EffectParticleDrawMode::Mesh) {
                    if (!effectiveParticle.meshAssetPath.empty()) {
                        if (auto model = ResourceManager::Instance().GetModel(effectiveParticle.meshAssetPath)) {
                            particleModelResource = model->GetModelResource();
                        }
                    }
                    if (!particleModelResource && meshes && meshes[row].model) {
                        particleModelResource = meshes[row].model->GetModelResource();
                    }
                    if (!particleModelResource) {
                        continue;
                    }
                }

                EffectParticlePacket packet;
                packet.runtimeInstanceId = playback.runtimeInstanceId;
                packet.drawMode = effectiveParticle.drawMode;
                packet.sortMode = effectiveParticle.sortMode;
                packet.shapeType = effectiveParticle.shapeType;
                packet.modelResource = particleModelResource;
                packet.origin = transform.worldPosition;
                packet.acceleration = effectiveParticle.acceleration;
                packet.drag = effectiveParticle.drag;
                packet.shapeParameters = effectiveParticle.shapeParameters;
                packet.spinRate = effectiveParticle.spinRate;
                packet.currentTime = playback.currentTime;
                packet.duration = effectiveDuration;
                packet.seed = playback.seed;
                packet.maxParticles = effectiveParticle.maxParticles;
                packet.spawnRate = effectiveParticle.spawnRate;
                packet.burstCount = effectiveParticle.burstCount;
                packet.particleLifetime = effectiveParticle.particleLifetime;
                packet.startSize = effectiveParticle.startSize;
                packet.endSize = effectiveParticle.endSize;
                packet.speed = effectiveParticle.speed;
                packet.lifetimeFade = playback.lifetimeFade;
                packet.ribbonWidth = effectiveParticle.ribbonWidth;
                packet.ribbonVelocityStretch = effectiveParticle.ribbonVelocityStretch;
                packet.sizeCurveBias = effectiveParticle.sizeCurveBias;
                packet.alphaCurveBias = effectiveParticle.alphaCurveBias;
                packet.subUvColumns = effectiveParticle.subUvColumns;
                packet.subUvRows = effectiveParticle.subUvRows;
                packet.subUvFrameRate = effectiveParticle.subUvFrameRate;
                packet.curlNoiseStrength = effectiveParticle.curlNoiseStrength;
                packet.curlNoiseScale = effectiveParticle.curlNoiseScale;
                packet.curlNoiseScrollSpeed = effectiveParticle.curlNoiseScrollSpeed;
                packet.vortexStrength = effectiveParticle.vortexStrength;
                packet.softParticleEnabled = effectiveParticle.softParticleEnabled;
                packet.softParticleScale = effectiveParticle.softParticleScale;
                packet.tint = effectiveParticle.tint;
                packet.tintEnd = effectiveParticle.tintEnd;
                packet.tint.w *= playback.lifetimeFade;
                packet.tintEnd.w *= playback.lifetimeFade;
                packet.boundsCenter = transform.worldPosition;
                packet.boundsExtents = { 0.5f, 0.5f, 0.5f };
                if (particleModelResource) {
                    const auto& localBounds = particleModelResource->GetLocalBounds();
                    packet.boundsCenter = {
                        transform.worldPosition.x + localBounds.Center.x,
                        transform.worldPosition.y + localBounds.Center.y,
                        transform.worldPosition.z + localBounds.Center.z
                    };
                    packet.boundsExtents = localBounds.Extents;
                }
                if (!effectiveParticle.texturePath.empty()) {
                    packet.texture = ResourceManager::Instance().GetTexture(effectiveParticle.texturePath);
                }
                queue.effectParticlePackets.push_back(std::move(packet));
            }
        }
    }

    queue.metrics.effectMeshPacketCount = static_cast<uint32_t>(queue.effectMeshPackets.size());
    queue.metrics.effectParticlePacketCount = static_cast<uint32_t>(queue.effectParticlePackets.size());
}
