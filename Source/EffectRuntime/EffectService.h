#pragma once

#include <cstdint>
#include <string>
#include <DirectXMath.h>

#include "Entity/Entity.h"

class Registry;

struct EffectHandle
{
    EntityID entity = Entity::NULL_ID;

    bool IsValid() const { return !Entity::IsNull(entity); }
    void Reset() { entity = Entity::NULL_ID; }
};

struct EffectPlayDesc
{
    std::string assetPath;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    uint32_t seed = 1;
    bool loop = false;
    const char* debugName = "Effect Runtime";
};

class EffectService
{
public:
    static EffectService& Instance();

    void SetRegistry(Registry* registry) { m_registry = registry; }
    Registry* GetRegistry() const { return m_registry; }

    EffectHandle PlayWorld(const EffectPlayDesc& desc);
    EffectHandle PlayWorld(Registry& registry, const EffectPlayDesc& desc);

    bool IsAlive(const EffectHandle& handle) const;
    bool IsAlive(Registry& registry, const EffectHandle& handle) const;

    void Stop(const EffectHandle& handle, bool destroyEntity = true);
    void Stop(Registry& registry, const EffectHandle& handle, bool destroyEntity = true);

    void Seek(const EffectHandle& handle, float time, float duration, bool loop);
    void Seek(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop);

    void SetWorldTransform(const EffectHandle& handle,
                           const DirectX::XMFLOAT3& position,
                           const DirectX::XMFLOAT4& rotation,
                           const DirectX::XMFLOAT3& scale);
    void SetWorldTransform(Registry& registry,
                           const EffectHandle& handle,
                           const DirectX::XMFLOAT3& position,
                           const DirectX::XMFLOAT4& rotation,
                           const DirectX::XMFLOAT3& scale);

    void SetWorldMatrix(const EffectHandle& handle, const DirectX::XMFLOAT4X4& worldMatrix);
    void SetWorldMatrix(Registry& registry, const EffectHandle& handle, const DirectX::XMFLOAT4X4& worldMatrix);

    void SetScalar(const EffectHandle& handle, const std::string& parameterName, float value);
    void SetScalar(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value);

    void SetColor(const EffectHandle& handle, const std::string& parameterName, const DirectX::XMFLOAT4& value);
    void SetColor(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const DirectX::XMFLOAT4& value);

private:
    EffectHandle PlayWorldInternal(Registry& registry, const EffectPlayDesc& desc);
    void StopInternal(Registry& registry, const EffectHandle& handle, bool destroyEntity);
    void SeekInternal(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop);
    void SetWorldTransformInternal(Registry& registry,
                                   const EffectHandle& handle,
                                   const DirectX::XMFLOAT3& position,
                                   const DirectX::XMFLOAT4& rotation,
                                   const DirectX::XMFLOAT3& scale);
    void SetScalarInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value);
    void SetColorInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const DirectX::XMFLOAT4& value);
    void EnsureBaseComponents(Registry& registry, EntityID entity, const char* debugName) const;

    Registry* m_registry = nullptr;
};
