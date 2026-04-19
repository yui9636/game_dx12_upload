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
#include "Component/HierarchyComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "EffectParameterBindings.h"
#include "EffectRuntimeRegistry.h"
#include "Registry/Registry.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include "Model/Model.h"
#include "Model/ModelResource.h"
#include "System/ResourceManager.h"
#include "Transform/NodeAttachmentUtils.h"
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

    void ApplyAttachedWorldToTransform(
        Registry& registry,
        EntityID entity,
        const DirectX::XMFLOAT4X4& desiredWorld)
    {
        using namespace DirectX;

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform) {
            return;
        }

        transform->prevWorldMatrix = transform->worldMatrix;
        transform->worldMatrix = desiredWorld;

        XMMATRIX parentWorld = XMMatrixIdentity();
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            if (!Entity::IsNull(hierarchy->parent)) {
                if (auto* parentTransform = registry.GetComponent<TransformComponent>(hierarchy->parent)) {
                    parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
                }
            }
        }

        XMFLOAT4X4 localMatrix;
        XMStoreFloat4x4(&localMatrix, XMLoadFloat4x4(&desiredWorld) * XMMatrixInverse(nullptr, parentWorld));
        transform->localMatrix = localMatrix;

        XMVECTOR localScale;
        XMVECTOR localRot;
        XMVECTOR localPos;
        if (XMMatrixDecompose(&localScale, &localRot, &localPos, XMLoadFloat4x4(&localMatrix))) {
            XMStoreFloat3(&transform->localScale, localScale);
            XMStoreFloat4(&transform->localRotation, localRot);
            XMStoreFloat3(&transform->localPosition, localPos);
        }

        XMVECTOR worldScale;
        XMVECTOR worldRot;
        XMVECTOR worldPos;
        if (XMMatrixDecompose(&worldScale, &worldRot, &worldPos, XMLoadFloat4x4(&desiredWorld))) {
            XMStoreFloat3(&transform->worldScale, worldScale);
            XMStoreFloat4(&transform->worldRotation, worldRot);
            XMStoreFloat3(&transform->worldPosition, worldPos);
        }

        transform->isDirty = false;
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
        for (size_t row = 0; row < archetype->GetEntityCount(); ++row) {
            const EntityID entity = archetype->GetEntities()[row];
            auto& attachment = attachments[row];
            if (Entity::IsNull(attachment.parentEntity) || !registry.IsAlive(attachment.parentEntity)) {
                continue;
            }

            auto* parentTransform = registry.GetComponent<TransformComponent>(attachment.parentEntity);
            if (!parentTransform) {
                continue;
            }

            DirectX::XMFLOAT4X4 desiredWorld{};
            bool resolved = false;

            if (!attachment.socketName.empty()) {
                auto* parentMesh = registry.GetComponent<MeshComponent>(attachment.parentEntity);
                if (parentMesh && parentMesh->model) {
                    auto* sockets = registry.GetComponent<NodeSocketComponent>(attachment.parentEntity);
                    int cachedBoneIndex = -1;
                    resolved = NodeAttachmentUtils::TryResolveNamedAttachmentWorldMatrix(
                        parentMesh->model.get(),
                        parentTransform->worldMatrix,
                        sockets,
                        attachment.socketName,
                        true,
                        cachedBoneIndex,
                        attachment.offsetLocal,
                        attachment.offsetRotDeg,
                        attachment.offsetScale,
                        NodeAttachmentSpace::NodeLocal,
                        desiredWorld);
                }
            }

            if (!resolved) {
                desiredWorld = NodeAttachmentUtils::ComposeAttachmentWorldMatrix(
                    parentTransform->worldMatrix,
                    attachment.offsetLocal,
                    attachment.offsetRotDeg,
                    attachment.offsetScale,
                    NodeAttachmentSpace::NodeLocal);
            }

            ApplyAttachedWorldToTransform(registry, entity, desiredWorld);
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
                // Multi-parameter overrides (Phase 1B)
                const size_t scalarCount = (std::min)(overrides[row].scalarNames.size(), overrides[row].scalarValues.size());
                for (size_t si = 0; si < scalarCount; ++si) {
                    if (!overrides[row].scalarNames[si].empty()) {
                        EffectParameterBindings::ApplyFloatParameter(
                            overrides[row].scalarNames[si],
                            overrides[row].scalarValues[si],
                            effectiveMesh,
                            effectiveParticle,
                            effectiveDuration);
                    }
                }
                const size_t colorCount = (std::min)(overrides[row].colorNames.size(), overrides[row].colorValues.size());
                for (size_t ci = 0; ci < colorCount; ++ci) {
                    if (!overrides[row].colorNames[ci].empty()) {
                        EffectParameterBindings::ApplyColorParameter(
                            overrides[row].colorNames[ci],
                            overrides[row].colorValues[ci],
                            effectiveMesh,
                            effectiveParticle);
                    }
                }
            }

            DirectX::XMFLOAT4 tint = effectiveMesh.tint;
            tint.w *= playback.lifetimeFade;

            if (effectiveMesh.enabled) {
                std::shared_ptr<ModelResource> modelResource;
                if (!effectiveMesh.meshAssetPath.empty()) {
                    auto model = ResourceManager::Instance().GetModel(effectiveMesh.meshAssetPath);
                    if (model) {
                        modelResource = model->GetModelResource();
                    }
                    LOG_INFO("[EffectMesh] meshAssetPath='%s' model=%p modelResource=%p",
                        effectiveMesh.meshAssetPath.c_str(), (void*)model.get(), (void*)modelResource.get());
                } else if (meshes && meshes[row].model) {
                    modelResource = meshes[row].model->GetModelResource();
                    LOG_INFO("[EffectMesh] fallback meshComponent model=%p", (void*)modelResource.get());
                } else {
                    LOG_WARN("[EffectMesh] No meshAssetPath and no MeshComponent fallback");
                }

                std::shared_ptr<MaterialAsset> materialAsset;
                if (!effectiveMesh.materialAssetPath.empty()) {
                    materialAsset = ResourceManager::Instance().GetMaterial(effectiveMesh.materialAssetPath);
                } else if (materials && !materials[row].materialAssetPath.empty()) {
                    materialAsset = materials[row].materialAsset
                        ? materials[row].materialAsset
                        : ResourceManager::Instance().GetMaterial(materials[row].materialAssetPath);
                }

                if (!modelResource) {
                    LOG_ERROR("[EffectMesh] modelResource null — packet skipped. meshAssetPath='%s'",
                        effectiveMesh.meshAssetPath.c_str());
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
                    packet.shaderVariantKey = effectiveMesh.shaderVariantKey != 0
                        ? effectiveMesh.shaderVariantKey
                        : effectiveMesh.variantParams.shaderFlags;
                    packet.lifetimeFade = playback.lifetimeFade;
                    // Phase A: variant params + alphaFade injection
                    packet.meshVariantParams = effectiveMesh.variantParams;
                    packet.meshVariantParams.constants.alphaFade = playback.lifetimeFade;
                    packet.meshVariantParams.constants.effectTime = playback.currentTime;
                    // Phase C (P0): When MeshFlag_Dissolve is enabled, drive
                    // dissolveAmount linearly from 0 to 1 over the effect's
                    // lifetime. lifetimeFade goes 1.0 -> 0.0 (see EffectSystems
                    // line ~63: lifetimeFade = 1 - normalized), so the progress
                    // 0.0 -> 1.0 is (1 - lifetimeFade). This gives slash-style
                    // templates an automatic "appear then dissolve" curve
                    // without keyframing. A future P1 can add an opt-out flag
                    // for templates that want a static dissolveAmount.
                    if (effectiveMesh.variantParams.shaderFlags & MeshFlag_Dissolve) {
                        packet.meshVariantParams.constants.dissolveAmount =
                            1.0f - playback.lifetimeFade;
                    }
                    // Variant textures
                    auto& vp = effectiveMesh.variantParams;
                    if (!vp.baseTexturePath.empty())
                        packet.baseTexture = ResourceManager::Instance().GetTexture(vp.baseTexturePath);
                    if (!vp.maskTexturePath.empty())
                        packet.maskTexture = ResourceManager::Instance().GetTexture(vp.maskTexturePath);
                    if (!vp.normalMapPath.empty())
                        packet.normalMapTexture = ResourceManager::Instance().GetTexture(vp.normalMapPath);
                    if (!vp.flowMapPath.empty())
                        packet.flowMapTexture = ResourceManager::Instance().GetTexture(vp.flowMapPath);
                    if (!vp.subTexturePath.empty())
                        packet.subTexture = ResourceManager::Instance().GetTexture(vp.subTexturePath);
                    if (!vp.emissionTexPath.empty())
                        packet.emissionTexture = ResourceManager::Instance().GetTexture(vp.emissionTexPath);
                    const float dx = transform.worldPosition.x - rc.cameraPosition.x;
                    const float dy = transform.worldPosition.y - rc.cameraPosition.y;
                    const float dz = transform.worldPosition.z - rc.cameraPosition.z;
                    packet.distanceToCamera = std::sqrt(dx * dx + dy * dy + dz * dz);
                    packet.sortKey = static_cast<uint64_t>(packet.distanceToCamera * 1000.0f);
                    LOG_INFO("[EffectMesh] packet pushed: mesh='%s' material='%s' shaderId=%d blend=%d variantKey=0x%08X materialAsset=%p tint=(%.2f,%.2f,%.2f,%.2f)",
                        effectiveMesh.meshAssetPath.c_str(),
                        effectiveMesh.materialAssetPath.c_str(),
                        packet.shaderId,
                        (int)packet.blendState,
                        packet.shaderVariantKey,
                        (void*)packet.materialAsset.get(),
                        packet.baseColor.x, packet.baseColor.y, packet.baseColor.z, packet.baseColor.w);
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
                packet.blendMode = effectiveParticle.blendMode;
                packet.randomSpeedRange = effectiveParticle.randomSpeedRange;
                packet.randomSizeRange = effectiveParticle.randomSizeRange;
                packet.randomLifeRange = effectiveParticle.randomLifeRange;
                packet.windStrength = effectiveParticle.windStrength;
                packet.windDirection = effectiveParticle.windDirection;
                packet.windTurbulence = effectiveParticle.windTurbulence;
                packet.sizeCurveValues = effectiveParticle.sizeCurveValues;
                packet.sizeCurveTimes = effectiveParticle.sizeCurveTimes;
                packet.sizeCurveKeyCount = effectiveParticle.sizeCurveKeyCount;
                packet.gradientColor0 = effectiveParticle.gradientColor0;
                packet.gradientColor1 = effectiveParticle.gradientColor1;
                packet.gradientColor2 = effectiveParticle.gradientColor2;
                packet.gradientColor3 = effectiveParticle.gradientColor3;
                packet.gradientMidTimes = effectiveParticle.gradientMidTimes;
                packet.gradientKeyCount = effectiveParticle.gradientKeyCount;
                for (int ai = 0; ai < 4; ++ai) packet.attractors[ai] = effectiveParticle.attractors[ai];
                packet.attractorRadii = effectiveParticle.attractorRadii;
                packet.attractorFalloff = effectiveParticle.attractorFalloff;
                packet.attractorCount = effectiveParticle.attractorCount;
                packet.collisionEnabled = effectiveParticle.collisionEnabled;
                packet.collisionPlane = effectiveParticle.collisionPlane;
                for (int ci = 0; ci < 4; ++ci) packet.collisionSpheres[ci] = effectiveParticle.collisionSpheres[ci];
                packet.collisionSphereCount = effectiveParticle.collisionSphereCount;
                packet.collisionRestitution = effectiveParticle.collisionRestitution;
                packet.collisionFriction = effectiveParticle.collisionFriction;
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
