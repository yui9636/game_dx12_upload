#include "CinematicService.h"

#include "Animator/AnimatorService.h"
#include "Archetype/Archetype.h"
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "CinematicSequenceSerializer.h"
#include "EffectRuntime/EffectService.h"
#include "Engine/EngineKernel.h"
#include "Engine/EngineTime.h"
#include "Message/MessageData.h"
#include "Message/Messenger.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace DirectX;

namespace
{
    CinematicSequenceAsset MakeFallbackSequence()
    {
        CinematicSequenceAsset asset;
        asset.name = "Missing Sequence";
        asset.frameRate = 60.0f;
        asset.durationFrames = 600;
        asset.playRangeStart = 0;
        asset.playRangeEnd = 600;
        asset.workRangeStart = 0;
        asset.workRangeEnd = 600;
        return asset;
    }

    XMFLOAT3 Lerp(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    XMFLOAT4 QuaternionFromEulerDegrees(const XMFLOAT3& eulerDeg)
    {
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(eulerDeg.x),
            XMConvertToRadians(eulerDeg.y),
            XMConvertToRadians(eulerDeg.z));
        XMFLOAT4 out;
        XMStoreFloat4(&out, XMQuaternionNormalize(q));
        return out;
    }

    XMFLOAT4 LookRotation(const XMFLOAT3& eye, const XMFLOAT3& target)
    {
        XMVECTOR eyeV = XMLoadFloat3(&eye);
        XMVECTOR targetV = XMLoadFloat3(&target);
        XMVECTOR forward = XMVector3Normalize(targetV - eyeV);
        const XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMMATRIX view = XMMatrixLookToLH(eyeV, forward, up);
        XMMATRIX invView = XMMatrixInverse(nullptr, view);
        XMVECTOR rot = XMQuaternionRotationMatrix(invView);
        XMFLOAT4 out;
        XMStoreFloat4(&out, XMQuaternionNormalize(rot));
        return out;
    }

    float FrameToNormalized(int frame, int start, int end)
    {
        if (end <= start) {
            return 0.0f;
        }
        return (std::clamp)(static_cast<float>(frame - start) / static_cast<float>(end - start), 0.0f, 1.0f);
    }

    bool IsInsideFrame(const CinematicSection& section, int frame)
    {
        return frame >= section.startFrame && frame <= section.endFrame;
    }

    bool RangeTouchesFrame(int a, int b, int frame)
    {
        const int minFrame = (std::min)(a, b);
        const int maxFrame = (std::max)(a, b);
        return frame >= minFrame && frame <= maxFrame;
    }

    float ApplyEase(float t, int currentFrame, const CinematicSection& section, float extraEaseInFrames = 0.0f, float extraEaseOutFrames = 0.0f)
    {
        const float sectionLength = static_cast<float>((std::max)(1, section.endFrame - section.startFrame));
        const float easeInFrames = (std::max)(0.0f, section.easeInFrames + extraEaseInFrames);
        const float easeOutFrames = (std::max)(0.0f, section.easeOutFrames + extraEaseOutFrames);

        if (easeInFrames <= 0.0f && easeOutFrames <= 0.0f) {
            return t;
        }

        const float localFrame = static_cast<float>(currentFrame - section.startFrame);
        float weight = t;

        if (easeInFrames > 0.0f && localFrame < easeInFrames) {
            const float inT = (std::clamp)(localFrame / (std::max)(1.0f, easeInFrames), 0.0f, 1.0f);
            weight = weight * (inT * inT * (3.0f - 2.0f * inT));
        }

        if (easeOutFrames > 0.0f) {
            const float framesToEnd = static_cast<float>(section.endFrame - currentFrame);
            if (framesToEnd < easeOutFrames) {
                const float outT = 1.0f - (std::clamp)(framesToEnd / (std::max)(1.0f, easeOutFrames), 0.0f, 1.0f);
                const float smoothOut = outT * outT * (3.0f - 2.0f * outT);
                weight = weight + (1.0f - weight) * smoothOut;
            }
        }

        return (std::clamp)(weight, 0.0f, 1.0f);
    }
}

CinematicService& CinematicService::Instance()
{
    static CinematicService instance;
    return instance;
}

void CinematicService::Update(const EngineTime& time)
{
    if (!m_registry) {
        return;
    }

    EffectService::Instance().SetRegistry(m_registry);
    AnimatorService::Instance().SetRegistry(m_registry);

    const float dt = (std::max)(time.dt, 0.0f);
    for (auto& [_, entry] : m_entries) {
        if (!entry.playing || entry.paused) {
            continue;
        }

        const float previousFrame = (entry.lastAppliedFrame < 0.0f) ? entry.currentFrame : entry.lastAppliedFrame;
        const int previousSpan = entry.playbackSpan;

        if (dt > 0.0f) {
            entry.currentFrame += dt * entry.asset.frameRate * entry.playbackRate;
            if (entry.currentFrame > static_cast<float>(entry.asset.playRangeEnd)) {
                entry.currentFrame = static_cast<float>(entry.asset.playRangeEnd);
                entry.playing = false;
            }
        }

        if (entry.dirty || std::fabs(entry.currentFrame - previousFrame) > 0.001f) {
            ApplyEntry(entry, previousFrame, previousSpan);
            entry.lastAppliedFrame = entry.currentFrame;
            entry.dirty = false;
        }
    }
}

CinematicSequenceHandle CinematicService::PlaySequence(const std::string& assetPath, const CinematicBindingContext& bindingContext)
{
    RuntimeEntry entry;
    entry.handle.id = m_nextHandleId++;
    entry.assetPath = assetPath;
    entry.bindingContext = bindingContext;
    entry.currentFrame = 0.0f;
    entry.playbackRate = 1.0f;
    entry.playing = true;
    entry.paused = false;
    entry.lastAppliedFrame = -1.0f;
    entry.playbackSpan = 0;
    entry.dirty = true;

    if (!assetPath.empty() && !CinematicSequenceSerializer::Load(assetPath, entry.asset)) {
        entry.asset = MakeFallbackSequence();
    }

    entry.currentFrame = static_cast<float>(entry.asset.playRangeStart);
    auto [it, _] = m_entries.emplace(entry.handle.id, std::move(entry));
    if (m_registry) {
        ApplyEntry(it->second, it->second.currentFrame, it->second.playbackSpan);
        it->second.lastAppliedFrame = it->second.currentFrame;
        it->second.dirty = false;
    }
    return CinematicSequenceHandle{ m_nextHandleId - 1 };
}

CinematicSequenceHandle CinematicService::PlaySequenceAsset(const CinematicSequenceAsset& asset, const CinematicBindingContext& bindingContext)
{
    RuntimeEntry entry;
    entry.handle.id = m_nextHandleId++;
    entry.bindingContext = bindingContext;
    entry.asset = asset;
    entry.currentFrame = static_cast<float>(asset.playRangeStart);
    entry.playbackRate = 1.0f;
    entry.playing = false;
    entry.paused = true;
    entry.lastAppliedFrame = -1.0f;
    entry.playbackSpan = 0;
    entry.dirty = true;

    auto [it, _] = m_entries.emplace(entry.handle.id, std::move(entry));
    if (m_registry) {
        ApplyEntry(it->second, it->second.currentFrame, it->second.playbackSpan);
        it->second.lastAppliedFrame = it->second.currentFrame;
        it->second.dirty = false;
    }
    return CinematicSequenceHandle{ m_nextHandleId - 1 };
}

void CinematicService::StopSequence(const CinematicSequenceHandle& handle)
{
    auto it = m_entries.find(handle.id);
    if (it == m_entries.end()) {
        return;
    }

    ClearEntry(it->second);
    m_entries.erase(it);
}

void CinematicService::PauseSequence(const CinematicSequenceHandle& handle, bool paused)
{
    if (RuntimeEntry* entry = Find(handle)) {
        entry->paused = paused;
        entry->dirty = true;
    }
}

void CinematicService::SeekSequence(const CinematicSequenceHandle& handle, float frame)
{
    RuntimeEntry* entry = Find(handle);
    if (!entry) {
        return;
    }

    const float previousFrame = (entry->lastAppliedFrame < 0.0f) ? entry->currentFrame : entry->lastAppliedFrame;
    const int previousSpan = entry->playbackSpan;
    entry->currentFrame = (std::clamp)(frame,
        static_cast<float>(entry->asset.playRangeStart),
        static_cast<float>(entry->asset.playRangeEnd));
    entry->dirty = true;

    if (m_registry) {
        ApplyEntry(*entry, previousFrame, previousSpan);
        entry->lastAppliedFrame = entry->currentFrame;
        entry->dirty = false;
    }
}

void CinematicService::SetPlaybackRate(const CinematicSequenceHandle& handle, float rate)
{
    if (RuntimeEntry* entry = Find(handle)) {
        entry->playbackRate = rate;
    }
}

void CinematicService::BindEntity(const CinematicSequenceHandle& handle, uint64_t bindingId, EntityID entity)
{
    if (RuntimeEntry* entry = Find(handle)) {
        entry->bindingContext.bindingOverrides[bindingId] = entity;
        entry->dirty = true;
    }
}

void CinematicService::UpdateSequenceAsset(const CinematicSequenceHandle& handle, const CinematicSequenceAsset& asset)
{
    if (RuntimeEntry* entry = Find(handle)) {
        if (m_registry) {
            ClearEntry(*entry);
        }
        entry->asset = asset;
        entry->currentFrame = (std::clamp)(entry->currentFrame,
            static_cast<float>(entry->asset.playRangeStart),
            static_cast<float>(entry->asset.playRangeEnd));
        entry->dirty = true;
    }
}

bool CinematicService::IsAlive(const CinematicSequenceHandle& handle) const
{
    return m_entries.find(handle.id) != m_entries.end();
}

const CinematicSequenceAsset* CinematicService::GetAsset(const CinematicSequenceHandle& handle) const
{
    const RuntimeEntry* entry = Find(handle);
    return entry ? &entry->asset : nullptr;
}

void CinematicService::RestoreMainCameraTags(RuntimeEntry& entry)
{
    if (!m_registry || !entry.capturedOriginalMainTaggedEntities) {
        return;
    }

    Registry& registry = *m_registry;
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const Signature signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<CameraLensComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<CameraMatricesComponent>())) {
            continue;
        }

        for (EntityID entity : archetype->GetEntities()) {
            const bool shouldBeMain = entry.originalMainTaggedEntities.find(entity) != entry.originalMainTaggedEntities.end();
            if (shouldBeMain) {
                if (!registry.GetComponent<CameraMainTagComponent>(entity)) {
                    registry.AddComponent(entity, CameraMainTagComponent{});
                }
            } else {
                registry.RemoveComponent<CameraMainTagComponent>(entity);
            }
        }
    }
}

void CinematicService::ApplyEntry(RuntimeEntry& entry, float previousFrameRaw, int previousSpan)
{
    if (!m_registry) {
        return;
    }

    Registry& registry = *m_registry;
    auto& animatorService = AnimatorService::Instance();
    auto& effectService = EffectService::Instance();
    auto& audioWorld = EngineKernel::Instance().GetAudioWorld();

    const int currentFrame = static_cast<int>(entry.currentFrame + 0.5f);
    const int previousFrame = static_cast<int>(previousFrameRaw + 0.5f);
    const bool isSeek = std::fabs(entry.currentFrame - previousFrameRaw) > 0.001f && (!entry.playing || entry.paused);

    struct EvalSegment
    {
        int start = 0;
        int end = 0;
        int span = 0;
        bool valid = false;
    };

    std::array<EvalSegment, 2> evalSegments = {};
    if (entry.playbackSpan != previousSpan && currentFrame < previousFrame) {
        evalSegments[0] = { previousFrame, entry.asset.playRangeEnd, previousSpan, true };
        evalSegments[1] = { entry.asset.playRangeStart, currentFrame, entry.playbackSpan, true };
    } else {
        evalSegments[0] = { previousFrame, currentFrame, entry.playbackSpan, true };
    }

    entry.enteredSectionsThisFrame.clear();
    entry.exitedSectionsThisFrame.clear();

    if (!entry.capturedOriginalMainTaggedEntities) {
        entry.originalMainTaggedEntities.clear();
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const Signature signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<CameraMainTagComponent>())) {
                continue;
            }
            for (EntityID entity : archetype->GetEntities()) {
                entry.originalMainTaggedEntities.insert(entity);
            }
        }
        entry.capturedOriginalMainTaggedEntities = true;
    }

    auto cleanupEffectHandles = [&](uint64_t sectionId) {
        auto it = entry.effectSectionHandles.find(sectionId);
        if (it == entry.effectSectionHandles.end()) {
            return;
        }
        auto& handles = it->second;
        handles.erase(std::remove_if(handles.begin(), handles.end(),
            [&](const EffectHandle& handle) {
                return !handle.IsValid() || !effectService.IsAlive(registry, handle);
            }), handles.end());
    };

    auto cleanupAudioHandles = [&](uint64_t sectionId) {
        auto it = entry.audioSectionHandles.find(sectionId);
        if (it == entry.audioSectionHandles.end()) {
            return;
        }
        auto& handles = it->second;
        handles.erase(std::remove_if(handles.begin(), handles.end(),
            [&](uint64_t handle) {
                return handle == 0 || !audioWorld.IsVoiceAlive(handle);
            }), handles.end());
    };

    auto stopEffectSectionHandles = [&](uint64_t sectionId) {
        auto it = entry.effectSectionHandles.find(sectionId);
        if (it == entry.effectSectionHandles.end()) {
            return;
        }
        for (const EffectHandle& handle : it->second) {
            if (handle.IsValid()) {
                effectService.Stop(registry, handle, true);
            }
        }
        it->second.clear();
    };

    auto stopAudioSectionHandles = [&](uint64_t sectionId) {
        auto it = entry.audioSectionHandles.find(sectionId);
        if (it == entry.audioSectionHandles.end()) {
            return;
        }
        for (uint64_t handle : it->second) {
            if (handle != 0) {
                audioWorld.StopVoice(handle);
            }
        }
        it->second.clear();
    };

    auto isEffectSectionActive = [&](uint64_t sectionId) {
        const auto it = entry.effectSectionHandles.find(sectionId);
        return it != entry.effectSectionHandles.end() && !it->second.empty();
    };

    auto isAudioSectionActive = [&](uint64_t sectionId) {
        const auto it = entry.audioSectionHandles.find(sectionId);
        return it != entry.audioSectionHandles.end() && !it->second.empty();
    };

    auto spawnEffectHandle = [&](const CinematicSection& section, const XMFLOAT3& position, const XMFLOAT4& rotation) {
        if (section.effect.effectAssetPath.empty()) {
            return;
        }

        EffectPlayDesc desc;
        desc.assetPath = section.effect.effectAssetPath;
        desc.position = position;
        desc.rotation = rotation;
        desc.scale = section.effect.offsetScale;
        desc.loop = section.effect.loop;
        desc.seed = section.effect.seed;
        desc.debugName = "Cinematic Effect";

        EffectHandle handle = effectService.PlayWorld(registry, desc);
        if (handle.IsValid()) {
            for (const CinematicScalarOverride& scalar : section.effect.assetOverrides) {
                effectService.SetScalar(registry, handle, scalar.name, scalar.value);
            }
            for (const CinematicScalarOverride& scalar : section.effect.editorPreviewScalarOverrides) {
                effectService.SetScalar(registry, handle, scalar.name, scalar.value);
            }
            for (const CinematicColorOverride& color : section.effect.colorOverrides) {
                effectService.SetColor(registry, handle, color.name, color.value);
            }
            entry.effectSectionHandles[section.sectionId].push_back(handle);
        }
    };

    auto spawnAudioHandle = [&](const CinematicSection& section, const XMFLOAT3& position) {
        if (section.audio.audioAssetPath.empty()) {
            return;
        }

        const uint64_t handle = section.audio.is3D
            ? audioWorld.PlayTransient3D(section.audio.audioAssetPath, position, section.audio.volume, section.audio.pitch, section.audio.loop)
            : audioWorld.PlayTransient2D(section.audio.audioAssetPath, section.audio.volume, section.audio.pitch, section.audio.loop);

        if (handle != 0) {
            entry.audioSectionHandles[section.sectionId].push_back(handle);
        }
    };

    auto evaluateEventLikeTriggers = [&](const CinematicTrack& track, const CinematicSection& section) {
        if (track.muted || section.muted || section.locked) {
            return;
        }

        for (const EvalSegment& segment : evalSegments) {
            if (!segment.valid || !RangeTouchesFrame(segment.start, segment.end, section.startFrame)) {
                continue;
            }
            if (isSeek && section.seekPolicy == CinematicSeekPolicy::SkipOnSeek) {
                continue;
            }

            const bool alreadyTriggered = section.eventData.fireOnce &&
                entry.triggeredSectionSpan.find(section.sectionId) != entry.triggeredSectionSpan.end() &&
                entry.triggeredSectionSpan[section.sectionId] == segment.span;

            if (alreadyTriggered) {
                continue;
            }

            if (track.type == CinematicTrackType::Event) {
                DispatchEvent(section);
            } else if (track.type == CinematicTrackType::CameraShake) {
                MessageData::CAMERASHAKEDATA data;
                data.shakeTimer = section.shake.duration;
                data.shakePower = section.shake.amplitude;
                Messenger::Instance().SendData(MessageData::CAMERASHAKE, &data);
            }

            entry.triggeredSectionSpan[section.sectionId] = segment.span;
        }
    };

    auto resolveBindingById = [&](uint64_t bindingId) -> EntityID {
        if (bindingId == 0) {
            return Entity::NULL_ID;
        }
        for (const CinematicBinding& binding : entry.asset.bindings) {
            if (binding.bindingId == bindingId) {
                return ResolveBindingEntity(entry, binding);
            }
        }
        return Entity::NULL_ID;
    };

    uint64_t activeCameraBindingId = 0;
    int activeCameraSectionStart = -1;
    for (const CinematicTrack& track : entry.asset.masterTracks) {
        if (track.muted || track.type != CinematicTrackType::Camera) {
            continue;
        }
        for (const CinematicSection& section : track.sections) {
            if (section.muted || section.locked || !IsInsideFrame(section, currentFrame) || section.camera.cameraBindingId == 0) {
                continue;
            }
            if (section.startFrame >= activeCameraSectionStart) {
                activeCameraSectionStart = section.startFrame;
                activeCameraBindingId = section.camera.cameraBindingId;
            }
        }
    }

    std::unordered_set<EntityID> animationDrivenNow;

    auto evaluateTrackSection = [&](EntityID entity, bool isCameraEntity, bool hasCameraTrackActive, const CinematicTrack& track, const CinematicSection& section) {
        const bool insideNow = IsInsideFrame(section, currentFrame);
        const float t = FrameToNormalized(currentFrame, section.startFrame, section.endFrame);

        switch (track.type) {
        case CinematicTrackType::Transform:
            if (insideNow && (!isCameraEntity || !hasCameraTrackActive)) {
                if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                    const float easedT = ApplyEase(t, currentFrame, section);
                    transform->localPosition = Lerp(section.transform.startPosition, section.transform.endPosition, easedT);
                    transform->localRotation = QuaternionFromEulerDegrees(Lerp(section.transform.startRotationEuler, section.transform.endRotationEuler, easedT));
                    transform->localScale = Lerp(section.transform.startScale, section.transform.endScale, easedT);
                    transform->isDirty = true;
                }
            }
            break;

        case CinematicTrackType::Camera:
            if (insideNow) {
                const float easedT = ApplyEase(t, currentFrame, section, section.camera.blendEaseIn, section.camera.blendEaseOut);
                if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                    if (section.camera.cameraMode == CinematicCameraMode::FreeCamera) {
                        transform->localPosition = Lerp(section.camera.startPosition, section.camera.endPosition, easedT);
                        transform->localRotation = QuaternionFromEulerDegrees(Lerp(section.camera.startRotationEuler, section.camera.endRotationEuler, easedT));
                    } else {
                        const XMFLOAT3 eye = Lerp(section.camera.startEye, section.camera.endEye, easedT);
                        const XMFLOAT3 target = Lerp(section.camera.startTarget, section.camera.endTarget, easedT);
                        transform->localPosition = eye;
                        transform->localRotation = LookRotation(eye, target);
                    }
                    transform->isDirty = true;
                }
                if (auto* lens = registry.GetComponent<CameraLensComponent>(entity)) {
                    lens->fovY = XMConvertToRadians(Lerp(section.camera.startFovDeg, section.camera.endFovDeg, easedT));
                }
            }
            break;

        case CinematicTrackType::Animation:
            if (insideNow) {
                animationDrivenNow.insert(entity);
                animatorService.EnsureAnimator(entity);
                animatorService.SetDriver(entity,
                    static_cast<float>(currentFrame - section.startFrame) / (std::max)(1.0f, entry.asset.frameRate),
                    section.animation.animationIndex,
                    section.animation.loop,
                    false);
            }
            break;

        case CinematicTrackType::Effect:
        {
            cleanupEffectHandles(section.sectionId);
            const bool wasActive = entry.activeSections.find(section.sectionId) != entry.activeSections.end();
            const bool allowSeekEnter = !isSeek || section.seekPolicy != CinematicSeekPolicy::SkipOnSeek;

            XMFLOAT3 worldPos = { 0.0f, 0.0f, 0.0f };
            if (const auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                worldPos = transform->worldPosition;
            }
            worldPos.x += section.effect.offsetPosition.x;
            worldPos.y += section.effect.offsetPosition.y;
            worldPos.z += section.effect.offsetPosition.z;
            const XMFLOAT4 effectRotation = QuaternionFromEulerDegrees(section.effect.offsetRotation);

            if (insideNow && allowSeekEnter && !wasActive && section.evalPolicy != CinematicEvalPolicy::TriggerOnly) {
                if (section.effect.retriggerPolicy == CinematicRetriggerPolicy::RestartIfActive) {
                    stopEffectSectionHandles(section.sectionId);
                }
                if (section.effect.retriggerPolicy != CinematicRetriggerPolicy::IgnoreIfActive || !isEffectSectionActive(section.sectionId)) {
                    spawnEffectHandle(section, worldPos, effectRotation);
                }
                entry.activeSections.insert(section.sectionId);
                entry.enteredSectionsThisFrame.insert(section.sectionId);
            }

            if (section.evalPolicy == CinematicEvalPolicy::TriggerOnly) {
                for (const EvalSegment& segment : evalSegments) {
                    if (!segment.valid || !RangeTouchesFrame(segment.start, segment.end, section.startFrame)) {
                        continue;
                    }
                    if (isSeek && section.seekPolicy == CinematicSeekPolicy::SkipOnSeek) {
                        continue;
                    }
                    if (section.effect.fireOnEnterOnly && entry.triggeredSectionSpan[section.sectionId] == segment.span) {
                        continue;
                    }
                    if (section.effect.retriggerPolicy == CinematicRetriggerPolicy::RestartIfActive) {
                        stopEffectSectionHandles(section.sectionId);
                    }
                    if (section.effect.retriggerPolicy != CinematicRetriggerPolicy::IgnoreIfActive || !isEffectSectionActive(section.sectionId)) {
                        spawnEffectHandle(section, worldPos, effectRotation);
                    }
                    entry.triggeredSectionSpan[section.sectionId] = segment.span;
                }
            }

            if (insideNow && isEffectSectionActive(section.sectionId)) {
                auto& handles = entry.effectSectionHandles[section.sectionId];
                const float duration = static_cast<float>((std::max)(1, section.endFrame - section.startFrame)) / (std::max)(1.0f, entry.asset.frameRate);
                const float localTime = static_cast<float>(currentFrame - section.startFrame) / (std::max)(1.0f, entry.asset.frameRate);
                for (const EffectHandle& handle : handles) {
                    effectService.Seek(registry, handle, localTime, duration, section.effect.loop);
                    effectService.SetWorldTransform(registry, handle, worldPos, effectRotation, section.effect.offsetScale);
                    for (const CinematicScalarOverride& scalar : section.effect.assetOverrides) {
                        effectService.SetScalar(registry, handle, scalar.name, scalar.value);
                    }
                    for (const CinematicScalarOverride& scalar : section.effect.editorPreviewScalarOverrides) {
                        effectService.SetScalar(registry, handle, scalar.name, scalar.value);
                    }
                    for (const CinematicColorOverride& color : section.effect.colorOverrides) {
                        effectService.SetColor(registry, handle, color.name, color.value);
                    }
                }
            }

            if (!insideNow && wasActive) {
                if (section.effect.stopOnExit) {
                    stopEffectSectionHandles(section.sectionId);
                }
                entry.activeSections.erase(section.sectionId);
                entry.exitedSectionsThisFrame.insert(section.sectionId);
            }
            break;
        }

        case CinematicTrackType::Audio:
        {
            cleanupAudioHandles(section.sectionId);
            const bool wasActive = entry.activeSections.find(section.sectionId) != entry.activeSections.end();
            const bool allowSeekEnter = !isSeek || section.seekPolicy != CinematicSeekPolicy::SkipOnSeek;

            XMFLOAT3 worldPos = { 0.0f, 0.0f, 0.0f };
            if (const auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                worldPos = transform->worldPosition;
            }

            if (insideNow && allowSeekEnter && !wasActive) {
                if (section.audio.retriggerPolicy == CinematicRetriggerPolicy::RestartIfActive) {
                    stopAudioSectionHandles(section.sectionId);
                }
                if (section.audio.retriggerPolicy != CinematicRetriggerPolicy::IgnoreIfActive || !isAudioSectionActive(section.sectionId)) {
                    spawnAudioHandle(section, worldPos);
                }
                entry.activeSections.insert(section.sectionId);
                entry.enteredSectionsThisFrame.insert(section.sectionId);
            }

            if (insideNow && isAudioSectionActive(section.sectionId) && section.audio.is3D) {
                for (uint64_t handle : entry.audioSectionHandles[section.sectionId]) {
                    audioWorld.SetVoicePosition(handle, worldPos);
                }
            }

            if (!insideNow && wasActive) {
                if (section.audio.stopOnExit) {
                    stopAudioSectionHandles(section.sectionId);
                }
                entry.activeSections.erase(section.sectionId);
                entry.exitedSectionsThisFrame.insert(section.sectionId);
            }
            break;
        }

        case CinematicTrackType::Event:
        case CinematicTrackType::CameraShake:
            evaluateEventLikeTriggers(track, section);
            break;

        default:
            break;
        }
    };

    for (const CinematicBinding& binding : entry.asset.bindings) {
        const EntityID entity = ResolveBindingEntity(entry, binding);
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            continue;
        }

        const bool isCameraEntity = registry.GetComponent<CameraLensComponent>(entity) != nullptr;
        bool hasCameraTrackActive = false;
        bool hasAnimationTrackActive = false;

        for (const CinematicTrack& track : binding.tracks) {
            if (track.muted || track.type != CinematicTrackType::Camera) {
                continue;
            }
            for (const CinematicSection& section : track.sections) {
                if (!section.muted && !section.locked && IsInsideFrame(section, currentFrame)) {
                    hasCameraTrackActive = true;
                    break;
                }
            }
            if (hasCameraTrackActive) {
                break;
            }
        }

        for (const CinematicTrack& track : binding.tracks) {
            if (track.muted) {
                continue;
            }

            for (const CinematicSection& section : track.sections) {
                if (section.muted || section.locked) {
                    continue;
                }
                if (track.type == CinematicTrackType::Animation && IsInsideFrame(section, currentFrame)) {
                    hasAnimationTrackActive = true;
                }
                evaluateTrackSection(entity, isCameraEntity, hasCameraTrackActive, track, section);
            }
        }

        if (!hasAnimationTrackActive) {
            animatorService.ClearDriver(entity);
        }
    }

    for (const CinematicTrack& track : entry.asset.masterTracks) {
        if (track.muted) {
            continue;
        }
        for (const CinematicSection& section : track.sections) {
            if (section.muted || section.locked) {
                continue;
            }

            switch (track.type) {
            case CinematicTrackType::Event:
            case CinematicTrackType::CameraShake:
                evaluateEventLikeTriggers(track, section);
                break;

            case CinematicTrackType::Audio:
            {
                cleanupAudioHandles(section.sectionId);
                const bool insideNow = IsInsideFrame(section, currentFrame);
                const bool wasActive = entry.activeSections.find(section.sectionId) != entry.activeSections.end();
                const bool allowSeekEnter = !isSeek || section.seekPolicy != CinematicSeekPolicy::SkipOnSeek;

                if (insideNow && allowSeekEnter && !wasActive) {
                    if (section.audio.retriggerPolicy == CinematicRetriggerPolicy::RestartIfActive) {
                        stopAudioSectionHandles(section.sectionId);
                    }
                    if (section.audio.retriggerPolicy != CinematicRetriggerPolicy::IgnoreIfActive || !isAudioSectionActive(section.sectionId)) {
                        spawnAudioHandle(section, { 0.0f, 0.0f, 0.0f });
                    }
                    entry.activeSections.insert(section.sectionId);
                    entry.enteredSectionsThisFrame.insert(section.sectionId);
                }

                if (!insideNow && wasActive) {
                    if (section.audio.stopOnExit) {
                        stopAudioSectionHandles(section.sectionId);
                    }
                    entry.activeSections.erase(section.sectionId);
                    entry.exitedSectionsThisFrame.insert(section.sectionId);
                }
                break;
            }

            default:
                break;
            }
        }
    }

    const EntityID activeCameraEntity = resolveBindingById(activeCameraBindingId);
    if (!Entity::IsNull(activeCameraEntity) && registry.IsAlive(activeCameraEntity)) {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const Signature signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<CameraLensComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<CameraMatricesComponent>())) {
                continue;
            }

            for (EntityID entity : archetype->GetEntities()) {
                if (entity == activeCameraEntity) {
                    if (!registry.GetComponent<CameraMainTagComponent>(entity)) {
                        registry.AddComponent(entity, CameraMainTagComponent{});
                    }
                } else {
                    registry.RemoveComponent<CameraMainTagComponent>(entity);
                }
            }
        }
    } else {
        RestoreMainCameraTags(entry);
    }

    for (EntityID entity : entry.animationDrivenEntities) {
        if (animationDrivenNow.find(entity) == animationDrivenNow.end()) {
            animatorService.ClearDriver(entity);
        }
    }
    entry.animationDrivenEntities = std::move(animationDrivenNow);
}

void CinematicService::ClearEntry(RuntimeEntry& entry)
{
    if (!m_registry) {
        return;
    }

    Registry& registry = *m_registry;
    auto& effectService = EffectService::Instance();
    auto& audioWorld = EngineKernel::Instance().GetAudioWorld();

    for (auto& [_, handles] : entry.effectSectionHandles) {
        for (const EffectHandle& handle : handles) {
            if (handle.IsValid()) {
                effectService.Stop(registry, handle, true);
            }
        }
    }
    for (auto& [_, handles] : entry.audioSectionHandles) {
        for (uint64_t handle : handles) {
            if (handle != 0) {
                audioWorld.StopVoice(handle);
            }
        }
    }
    for (EntityID entity : entry.animationDrivenEntities) {
        AnimatorService::Instance().ClearDriver(entity);
    }

    entry.activeSections.clear();
    entry.enteredSectionsThisFrame.clear();
    entry.exitedSectionsThisFrame.clear();
    entry.triggeredSectionSpan.clear();
    entry.effectSectionHandles.clear();
    entry.audioSectionHandles.clear();
    entry.animationDrivenEntities.clear();
    RestoreMainCameraTags(entry);
    entry.originalMainTaggedEntities.clear();
    entry.capturedOriginalMainTaggedEntities = false;
}

EntityID CinematicService::ResolveBindingEntity(const RuntimeEntry& entry, const CinematicBinding& binding) const
{
    const auto it = entry.bindingContext.bindingOverrides.find(binding.bindingId);
    if (it != entry.bindingContext.bindingOverrides.end()) {
        return it->second;
    }
    return binding.targetEntity;
}

void CinematicService::DispatchEvent(const CinematicSection& section) const
{
    MessageData::CINEMATIC_EVENT_TRIGGER_DATA data;
    data.eventName = section.eventData.eventName;
    data.eventCategory = section.eventData.eventCategory;
    data.payloadType = section.eventData.payloadType;
    data.payloadJson = section.eventData.payloadJson;
    Messenger::Instance().SendData(MessageData::CINEMATIC_EVENT_TRIGGER, &data);
}

CinematicService::RuntimeEntry* CinematicService::Find(const CinematicSequenceHandle& handle)
{
    auto it = m_entries.find(handle.id);
    return it != m_entries.end() ? &it->second : nullptr;
}

const CinematicService::RuntimeEntry* CinematicService::Find(const CinematicSequenceHandle& handle) const
{
    auto it = m_entries.find(handle.id);
    return it != m_entries.end() ? &it->second : nullptr;
}
