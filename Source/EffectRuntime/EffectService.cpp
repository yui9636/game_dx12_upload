#include "EffectService.h"

#include <algorithm>

#include "Component/EffectAssetComponent.h"
#include "Component/EffectParameterOverrideComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/EffectSpawnRequestComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "EffectRuntimeRegistry.h"
#include "Registry/Registry.h"

using namespace DirectX;

namespace
{
    float ComputeLifetimeFade(float currentTime, float duration, bool loop, bool isPlaying)
    {
        if (!isPlaying) {
            return 0.0f;
        }

        if (duration <= 0.0f || loop) {
            return 1.0f;
        }

        const float normalized = std::clamp(currentTime / duration, 0.0f, 1.0f);
        return 1.0f - normalized;
    }

    bool DecomposeWorldMatrix(const XMFLOAT4X4& worldMatrix,
                              XMFLOAT3& outPosition,
                              XMFLOAT4& outRotation,
                              XMFLOAT3& outScale)
    {
        const XMMATRIX world = XMLoadFloat4x4(&worldMatrix);
        XMVECTOR scale;
        XMVECTOR rotation;
        XMVECTOR translation;
        if (!XMMatrixDecompose(&scale, &rotation, &translation, world)) {
            return false;
        }

        XMStoreFloat3(&outPosition, translation);
        XMStoreFloat4(&outRotation, rotation);
        XMStoreFloat3(&outScale, scale);
        return true;
    }
}

EffectService& EffectService::Instance()
{
    static EffectService instance;
    return instance;
}

EffectHandle EffectService::PlayWorld(const EffectPlayDesc& desc)
{
    return m_registry ? PlayWorldInternal(*m_registry, desc) : EffectHandle{};
}

EffectHandle EffectService::PlayWorld(Registry& registry, const EffectPlayDesc& desc)
{
    return PlayWorldInternal(registry, desc);
}

bool EffectService::IsAlive(const EffectHandle& handle) const
{
    return m_registry ? IsAlive(*m_registry, handle) : false;
}

bool EffectService::IsAlive(Registry& registry, const EffectHandle& handle) const
{
    return handle.IsValid() && registry.IsAlive(handle.entity);
}

void EffectService::Stop(const EffectHandle& handle, bool destroyEntity)
{
    if (m_registry) {
        StopInternal(*m_registry, handle, destroyEntity);
    }
}

void EffectService::Stop(Registry& registry, const EffectHandle& handle, bool destroyEntity)
{
    StopInternal(registry, handle, destroyEntity);
}

void EffectService::Seek(const EffectHandle& handle, float time, float duration, bool loop)
{
    if (m_registry) {
        SeekInternal(*m_registry, handle, time, duration, loop);
    }
}

void EffectService::Seek(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop)
{
    SeekInternal(registry, handle, time, duration, loop);
}

void EffectService::SetWorldTransform(const EffectHandle& handle,
                                      const XMFLOAT3& position,
                                      const XMFLOAT4& rotation,
                                      const XMFLOAT3& scale)
{
    if (m_registry) {
        SetWorldTransformInternal(*m_registry, handle, position, rotation, scale);
    }
}

void EffectService::SetWorldTransform(Registry& registry,
                                      const EffectHandle& handle,
                                      const XMFLOAT3& position,
                                      const XMFLOAT4& rotation,
                                      const XMFLOAT3& scale)
{
    SetWorldTransformInternal(registry, handle, position, rotation, scale);
}

void EffectService::SetWorldMatrix(const EffectHandle& handle, const XMFLOAT4X4& worldMatrix)
{
    if (!m_registry) {
        return;
    }
    SetWorldMatrix(*m_registry, handle, worldMatrix);
}

void EffectService::SetWorldMatrix(Registry& registry, const EffectHandle& handle, const XMFLOAT4X4& worldMatrix)
{
    XMFLOAT3 position;
    XMFLOAT4 rotation;
    XMFLOAT3 scale;
    if (!DecomposeWorldMatrix(worldMatrix, position, rotation, scale)) {
        return;
    }
    SetWorldTransformInternal(registry, handle, position, rotation, scale);
}

void EffectService::SetScalar(const EffectHandle& handle, const std::string& parameterName, float value)
{
    if (m_registry) {
        SetScalarInternal(*m_registry, handle, parameterName, value);
    }
}

void EffectService::SetScalar(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value)
{
    SetScalarInternal(registry, handle, parameterName, value);
}

void EffectService::SetColor(const EffectHandle& handle, const std::string& parameterName, const XMFLOAT4& value)
{
    if (m_registry) {
        SetColorInternal(*m_registry, handle, parameterName, value);
    }
}

void EffectService::SetColor(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const XMFLOAT4& value)
{
    SetColorInternal(registry, handle, parameterName, value);
}

EffectHandle EffectService::PlayWorldInternal(Registry& registry, const EffectPlayDesc& desc)
{
    if (desc.assetPath.empty()) {
        return {};
    }

    EffectHandle handle;
    handle.entity = registry.CreateEntity();
    EnsureBaseComponents(registry, handle.entity, desc.debugName);

    auto* transform = registry.GetComponent<TransformComponent>(handle.entity);
    if (transform) {
        transform->localPosition = desc.position;
        transform->localRotation = desc.rotation;
        transform->localScale = desc.scale;
        transform->isDirty = true;
    }

    EffectAssetComponent assetComponent;
    assetComponent.assetPath = desc.assetPath;
    assetComponent.autoPlay = true;
    assetComponent.loop = desc.loop;
    assetComponent.useSelectedMeshFallback = true;

    EffectPlaybackComponent playbackComponent;
    playbackComponent.seed = desc.seed;
    playbackComponent.loop = desc.loop;
    playbackComponent.currentTime = 0.0f;
    playbackComponent.duration = 2.0f;
    playbackComponent.lifetimeFade = 1.0f;

    EffectSpawnRequestComponent requestComponent;
    requestComponent.pending = true;
    requestComponent.restartIfActive = true;

    registry.AddComponent(handle.entity, assetComponent);
    registry.AddComponent(handle.entity, playbackComponent);
    registry.AddComponent(handle.entity, requestComponent);
    return handle;
}

void EffectService::StopInternal(Registry& registry, const EffectHandle& handle, bool destroyEntity)
{
    if (!IsAlive(registry, handle)) {
        return;
    }

    if (auto* playback = registry.GetComponent<EffectPlaybackComponent>(handle.entity)) {
        if (playback->runtimeInstanceId != 0) {
            EffectRuntimeRegistry::Instance().Destroy(playback->runtimeInstanceId);
            playback->runtimeInstanceId = 0;
        }
        playback->isPlaying = false;
        playback->stopRequested = true;
        playback->lifetimeFade = 0.0f;
    }

    if (destroyEntity) {
        registry.DestroyEntity(handle.entity);
    }
}

void EffectService::SeekInternal(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop)
{
    if (!IsAlive(registry, handle)) {
        return;
    }

    auto* playback = registry.GetComponent<EffectPlaybackComponent>(handle.entity);
    if (!playback) {
        return;
    }

    const float clampedTime = time < 0.0f ? 0.0f : time;
    playback->currentTime = clampedTime;
    if (duration > 0.0f) {
        playback->duration = duration;
    }
    playback->loop = loop;
    playback->isPlaying = true;
    playback->stopRequested = false;
    playback->lifetimeFade = ComputeLifetimeFade(playback->currentTime, playback->duration, playback->loop, playback->isPlaying);

    if (auto* request = registry.GetComponent<EffectSpawnRequestComponent>(handle.entity)) {
        request->startTime = clampedTime;
    }

    if (playback->runtimeInstanceId != 0) {
        if (auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback->runtimeInstanceId)) {
            runtime->time = playback->currentTime;
        }
    }
}

void EffectService::SetWorldTransformInternal(Registry& registry,
                                              const EffectHandle& handle,
                                              const XMFLOAT3& position,
                                              const XMFLOAT4& rotation,
                                              const XMFLOAT3& scale)
{
    if (!IsAlive(registry, handle)) {
        return;
    }

    auto* transform = registry.GetComponent<TransformComponent>(handle.entity);
    if (!transform) {
        return;
    }

    transform->localPosition = position;
    transform->localRotation = rotation;
    transform->localScale = scale;
    transform->isDirty = true;
}

void EffectService::SetScalarInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value)
{
    if (!IsAlive(registry, handle) || parameterName.empty()) {
        return;
    }

    auto* overrideComponent = registry.GetComponent<EffectParameterOverrideComponent>(handle.entity);
    if (!overrideComponent) {
        registry.AddComponent(handle.entity, EffectParameterOverrideComponent{});
        overrideComponent = registry.GetComponent<EffectParameterOverrideComponent>(handle.entity);
    }
    if (!overrideComponent) {
        return;
    }

    overrideComponent->enabled = true;
    overrideComponent->scalarParameter = parameterName;
    overrideComponent->scalarValue = value;
}

void EffectService::SetColorInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const XMFLOAT4& value)
{
    if (!IsAlive(registry, handle) || parameterName.empty()) {
        return;
    }

    auto* overrideComponent = registry.GetComponent<EffectParameterOverrideComponent>(handle.entity);
    if (!overrideComponent) {
        registry.AddComponent(handle.entity, EffectParameterOverrideComponent{});
        overrideComponent = registry.GetComponent<EffectParameterOverrideComponent>(handle.entity);
    }
    if (!overrideComponent) {
        return;
    }

    overrideComponent->enabled = true;
    overrideComponent->colorParameter = parameterName;
    overrideComponent->colorValue = value;
}

void EffectService::EnsureBaseComponents(Registry& registry, EntityID entity, const char* debugName) const
{
    registry.AddComponent(entity, NameComponent{ debugName ? debugName : "Effect Runtime" });
    registry.AddComponent(entity, HierarchyComponent{});
    registry.AddComponent(entity, TransformComponent{});
}
