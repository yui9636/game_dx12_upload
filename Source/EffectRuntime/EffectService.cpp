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
    // 現在時刻と総再生時間から lifetime fade 値を計算する。
    // 停止中なら 0、ループ中または duration 無効なら常に 1 を返す。
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

    // worldMatrix を position / rotation / scale へ分解する。
    // 分解に失敗した場合は false を返す。
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

// singleton インスタンスを返す。
EffectService& EffectService::Instance()
{
    static EffectService instance;
    return instance;
}

// 内部 Registry が設定済みなら、その Registry 上でワールドエフェクトを再生する。
EffectHandle EffectService::PlayWorld(const EffectPlayDesc& desc)
{
    return m_registry ? PlayWorldInternal(*m_registry, desc) : EffectHandle{};
}

// 明示された Registry 上でワールドエフェクトを再生する。
EffectHandle EffectService::PlayWorld(Registry& registry, const EffectPlayDesc& desc)
{
    return PlayWorldInternal(registry, desc);
}

// 内部 Registry がある場合、その handle が生存しているか判定する。
bool EffectService::IsAlive(const EffectHandle& handle) const
{
    return m_registry ? IsAlive(*m_registry, handle) : false;
}

// 指定 Registry 上で、その handle が有効かつ entity が生存しているか判定する。
bool EffectService::IsAlive(Registry& registry, const EffectHandle& handle) const
{
    return handle.IsValid() && registry.IsAlive(handle.entity);
}

// 内部 Registry がある場合、その effect を停止する。
void EffectService::Stop(const EffectHandle& handle, bool destroyEntity)
{
    if (m_registry) {
        StopInternal(*m_registry, handle, destroyEntity);
    }
}

// 明示された Registry 上で、その effect を停止する。
void EffectService::Stop(Registry& registry, const EffectHandle& handle, bool destroyEntity)
{
    StopInternal(registry, handle, destroyEntity);
}

// 内部 Registry がある場合、その effect の再生位置を変更する。
void EffectService::Seek(const EffectHandle& handle, float time, float duration, bool loop)
{
    if (m_registry) {
        SeekInternal(*m_registry, handle, time, duration, loop);
    }
}

// 明示された Registry 上で、その effect の再生位置を変更する。
void EffectService::Seek(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop)
{
    SeekInternal(registry, handle, time, duration, loop);
}

// 内部 Registry がある場合、その effect の transform を直接設定する。
void EffectService::SetWorldTransform(const EffectHandle& handle,
    const XMFLOAT3& position,
    const XMFLOAT4& rotation,
    const XMFLOAT3& scale)
{
    if (m_registry) {
        SetWorldTransformInternal(*m_registry, handle, position, rotation, scale);
    }
}

// 明示された Registry 上で、その effect の transform を直接設定する。
void EffectService::SetWorldTransform(Registry& registry,
    const EffectHandle& handle,
    const XMFLOAT3& position,
    const XMFLOAT4& rotation,
    const XMFLOAT3& scale)
{
    SetWorldTransformInternal(registry, handle, position, rotation, scale);
}

// 内部 Registry がある場合、行列から transform を分解して effect に設定する。
void EffectService::SetWorldMatrix(const EffectHandle& handle, const XMFLOAT4X4& worldMatrix)
{
    if (!m_registry) {
        return;
    }
    SetWorldMatrix(*m_registry, handle, worldMatrix);
}

// 明示された Registry 上で、行列から transform を分解して effect に設定する。
void EffectService::SetWorldMatrix(Registry& registry, const EffectHandle& handle, const XMFLOAT4X4& worldMatrix)
{
    XMFLOAT3 position;
    XMFLOAT4 rotation;
    XMFLOAT3 scale;

    // 行列分解に失敗したら何もしない。
    if (!DecomposeWorldMatrix(worldMatrix, position, rotation, scale)) {
        return;
    }

    SetWorldTransformInternal(registry, handle, position, rotation, scale);
}

// 内部 Registry がある場合、float パラメータ override を設定する。
void EffectService::SetScalar(const EffectHandle& handle, const std::string& parameterName, float value)
{
    if (m_registry) {
        SetScalarInternal(*m_registry, handle, parameterName, value);
    }
}

// 明示された Registry 上で、float パラメータ override を設定する。
void EffectService::SetScalar(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value)
{
    SetScalarInternal(registry, handle, parameterName, value);
}

// 内部 Registry がある場合、color パラメータ override を設定する。
void EffectService::SetColor(const EffectHandle& handle, const std::string& parameterName, const XMFLOAT4& value)
{
    if (m_registry) {
        SetColorInternal(*m_registry, handle, parameterName, value);
    }
}

// 明示された Registry 上で、color パラメータ override を設定する。
void EffectService::SetColor(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const XMFLOAT4& value)
{
    SetColorInternal(registry, handle, parameterName, value);
}

// 実際に effect entity を新規生成して、再生に必要な component を付与する。
EffectHandle EffectService::PlayWorldInternal(Registry& registry, const EffectPlayDesc& desc)
{
    // アセットパスが無ければ生成できない。
    if (desc.assetPath.empty()) {
        return {};
    }

    EffectHandle handle;
    handle.entity = registry.CreateEntity();

    // 最低限必要な基本 component を付与する。
    EnsureBaseComponents(registry, handle.entity, desc.debugName);

    // 初期 transform を設定する。
    auto* transform = registry.GetComponent<TransformComponent>(handle.entity);
    if (transform) {
        transform->localPosition = desc.position;
        transform->localRotation = desc.rotation;
        transform->localScale = desc.scale;
        transform->isDirty = true;
    }

    // effect アセット参照 component を作る。
    EffectAssetComponent assetComponent;
    assetComponent.assetPath = desc.assetPath;
    assetComponent.autoPlay = true;
    assetComponent.loop = desc.loop;
    assetComponent.useSelectedMeshFallback = true;

    // 再生状態 component を初期化する。
    EffectPlaybackComponent playbackComponent;
    playbackComponent.seed = desc.seed;
    playbackComponent.loop = desc.loop;
    playbackComponent.currentTime = 0.0f;
    playbackComponent.duration = 2.0f;
    playbackComponent.lifetimeFade = 1.0f;

    // 次フレーム以降で実際に spawn させる要求 component。
    EffectSpawnRequestComponent requestComponent;
    requestComponent.pending = true;
    requestComponent.restartIfActive = true;

    // まとめて component を付与する。
    registry.AddComponent(handle.entity, assetComponent);
    registry.AddComponent(handle.entity, playbackComponent);
    registry.AddComponent(handle.entity, requestComponent);

    return handle;
}

// effect を停止する。
// runtime instance があれば破棄し、必要なら entity 自体も削除する。
void EffectService::StopInternal(Registry& registry, const EffectHandle& handle, bool destroyEntity)
{
    if (!IsAlive(registry, handle)) {
        return;
    }

    if (auto* playback = registry.GetComponent<EffectPlaybackComponent>(handle.entity)) {
        // runtime instance が走っていれば先に破棄する。
        if (playback->runtimeInstanceId != 0) {
            EffectRuntimeRegistry::Instance().Destroy(playback->runtimeInstanceId);
            playback->runtimeInstanceId = 0;
        }

        playback->isPlaying = false;
        playback->stopRequested = true;
        playback->lifetimeFade = 0.0f;
    }

    // destroy 指定なら entity 自体を消す。
    if (destroyEntity) {
        registry.DestroyEntity(handle.entity);
    }
}

// effect の再生位置を変更する。
// 必要なら duration や loop も更新し、runtime instance 側の time も同期する。
void EffectService::SeekInternal(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop)
{
    if (!IsAlive(registry, handle)) {
        return;
    }

    auto* playback = registry.GetComponent<EffectPlaybackComponent>(handle.entity);
    if (!playback) {
        return;
    }

    // time は 0 未満にならないように丸める。
    const float clampedTime = time < 0.0f ? 0.0f : time;

    playback->currentTime = clampedTime;

    // 明示指定された duration が正なら更新する。
    if (duration > 0.0f) {
        playback->duration = duration;
    }

    playback->loop = loop;
    playback->isPlaying = true;
    playback->stopRequested = false;

    // 新しい再生位置に応じて lifetime fade を再計算する。
    playback->lifetimeFade = ComputeLifetimeFade(playback->currentTime, playback->duration, playback->loop, playback->isPlaying);

    // SpawnRequest があるなら開始位置も同期する。
    if (auto* request = registry.GetComponent<EffectSpawnRequestComponent>(handle.entity)) {
        request->startTime = clampedTime;
    }

    // 既に runtime instance があるなら、その time も同期する。
    if (playback->runtimeInstanceId != 0) {
        if (auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback->runtimeInstanceId)) {
            runtime->time = playback->currentTime;
        }
    }
}

// effect entity の TransformComponent を直接更新する。
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

// float パラメータ override を設定する。
// Override component が無ければその場で追加する。
void EffectService::SetScalarInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value)
{
    if (!IsAlive(registry, handle) || parameterName.empty()) {
        return;
    }

    auto* overrideComponent = registry.GetComponent<EffectParameterOverrideComponent>(handle.entity);

    // 無ければ追加する。
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

// color パラメータ override を設定する。
// Override component が無ければその場で追加する。
void EffectService::SetColorInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const XMFLOAT4& value)
{
    if (!IsAlive(registry, handle) || parameterName.empty()) {
        return;
    }

    auto* overrideComponent = registry.GetComponent<EffectParameterOverrideComponent>(handle.entity);

    // 無ければ追加する。
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

// effect runtime entity に最低限必要な基本 component を付与する。
void EffectService::EnsureBaseComponents(Registry& registry, EntityID entity, const char* debugName) const
{
    registry.AddComponent(entity, NameComponent{ debugName ? debugName : "Effect Runtime" });
    registry.AddComponent(entity, HierarchyComponent{});
    registry.AddComponent(entity, TransformComponent{});
}